#include "minidb/page.hpp"
#include "minidb/record.hpp"
#include "minidb/table.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <stdexcept>
#include <vector>
#include <numeric>

namespace {

using Clock = std::chrono::steady_clock;

double milliseconds(
    Clock::time_point start,
    Clock::time_point end
) {
    return std::chrono::duration<double, std::milli>(
        end - start
    ).count();
}

double microseconds(
    Clock::time_point start,
    Clock::time_point end
) {
    return std::chrono::duration<double, std::micro>(
        end - start
    ).count();
}

double percentile(
    std::vector<double> values,
    double p
) {
    if (values.empty()) {
        return 0.0;
    }

    std::sort(values.begin(), values.end());

    const std::size_t index =
        static_cast<std::size_t>(
            std::ceil(
                p * static_cast<double>(values.size() - 1)
            )
        );

    return values[index];
}

struct Results {
    std::size_t records{};
    double insertMs{};
    double insertRps{};

    std::size_t lookupCount{};
    double lookupAvgUs{};
    double lookupP50Us{};
    double lookupP95Us{};
    double lookupP99Us{};

    double scanMs{};
    double scanRps{};

    double reopenMs{};

    std::uint32_t pages{};
    std::uint64_t fileBytes{};
    std::size_t logicalRecordBytes{};
    double bytesPerRecord{};
    double recordsPerPage{};
    double payloadUtilizationPct{};
    double physicalUsedPct{};

    std::size_t theoreticalRecordsPerFullPage{};
    double theoreticalFullPageUtilizationPct{};
};

Results runBenchmark(std::size_t recordCount) {
    const std::filesystem::path dbPath =
        "benchmark_" + std::to_string(recordCount) + ".db";

    std::filesystem::remove(dbPath);

    Results result;
    result.records = recordCount;

    // Exactly 16 characters -> serialized record size = 32 bytes:
    // id(4) + nameLength(4) + name(16) + age(4) + score(4)
    const std::string fixedName = "abcdefghijklmnop";

    const std::size_t serializedBytesPerRecord =
        sizeof(std::int32_t)
        + sizeof(std::uint32_t)
        + fixedName.size()
        + sizeof(std::int32_t)
        + sizeof(std::int32_t);

    result.theoreticalRecordsPerFullPage =
        (minidb::PAGE_SIZE - minidb::PAGE_HEADER_SIZE)
        / (serializedBytesPerRecord + minidb::SLOT_SIZE);

    const std::size_t usedOnFullPage =
        minidb::PAGE_HEADER_SIZE
        + result.theoreticalRecordsPerFullPage
            * (serializedBytesPerRecord + minidb::SLOT_SIZE);

    result.theoreticalFullPageUtilizationPct =
        100.0
        * static_cast<double>(usedOnFullPage)
        / static_cast<double>(minidb::PAGE_SIZE);

    {
        minidb::Table table(dbPath.string());

        if (!table.isReady()) {
            throw std::runtime_error(
                "Could not create benchmark database."
            );
        }

        const auto insertStart = Clock::now();

        for (std::size_t i = 0; i < recordCount; ++i) {
            minidb::Record record{
                static_cast<std::int32_t>(i),
                fixedName,
                static_cast<std::int32_t>(18 + (i % 50)),
                static_cast<std::int32_t>(i % 1000000)
            };

            if (!table.insert(record)) {
                throw std::runtime_error(
                    "Insert failed at record "
                    + std::to_string(i)
                );
            }
        }

        const auto insertEnd = Clock::now();

        result.insertMs =
            milliseconds(insertStart, insertEnd);

        result.insertRps =
            result.insertMs > 0.0
            ? 1000.0 * static_cast<double>(recordCount)
                / result.insertMs
            : 0.0;
    }

    // Reopen timing includes reconstruction of the in-memory id -> RID index.
    const auto reopenStart = Clock::now();

    minidb::Table table(dbPath.string());

    const auto reopenEnd = Clock::now();

    if (!table.isReady()) {
        throw std::runtime_error(
            "Could not reopen benchmark database."
        );
    }

    result.reopenMs =
        milliseconds(reopenStart, reopenEnd);

    const auto stats = table.stats();

    result.pages = stats.pageCount;
    result.fileBytes = stats.fileBytes;
    result.logicalRecordBytes = stats.logicalRecordBytes;

    result.bytesPerRecord =
        recordCount > 0
        ? static_cast<double>(result.fileBytes)
            / static_cast<double>(recordCount)
        : 0.0;

    result.recordsPerPage =
        result.pages > 0
        ? static_cast<double>(recordCount)
            / static_cast<double>(result.pages)
        : 0.0;

    result.payloadUtilizationPct =
        result.fileBytes > 0
        ? 100.0
            * static_cast<double>(result.logicalRecordBytes)
            / static_cast<double>(result.fileBytes)
        : 0.0;

    const std::uint64_t metadataBytes =
        static_cast<std::uint64_t>(result.pages)
            * minidb::PAGE_HEADER_SIZE
        + static_cast<std::uint64_t>(recordCount)
            * minidb::SLOT_SIZE;

    result.physicalUsedPct =
        result.fileBytes > 0
        ? 100.0
            * static_cast<double>(
                result.logicalRecordBytes + metadataBytes
            )
            / static_cast<double>(result.fileBytes)
        : 0.0;

    // Warm the OS/filesystem cache and the table's hash index.
    const std::size_t warmupCount =
        std::min<std::size_t>(recordCount, 1000);

    std::uint64_t checksum = 0;

    for (std::size_t i = 0; i < warmupCount; ++i) {
        const std::size_t id =
            (i * recordCount) / warmupCount;

        auto record =
            table.get(
                static_cast<std::int32_t>(id)
            );

        if (!record) {
            throw std::runtime_error(
                "Warm-up lookup failed."
            );
        }

        checksum +=
            static_cast<std::uint64_t>(
                record->score
            );
    }

    // Measure up to 10,000 random point lookups.
    result.lookupCount =
        std::min<std::size_t>(
            recordCount,
            10000
        );
    
    std::vector<std::int32_t> allIds(recordCount);
    
    std::iota(
        allIds.begin(),
        allIds.end(),
        0
    );
    
    std::mt19937 rng(42);
    
    std::shuffle(
        allIds.begin(),
        allIds.end(),
        rng
    );
    
    std::vector<std::int32_t> lookupIds(
        allIds.begin(),
        allIds.begin() + result.lookupCount
    );

    std::vector<double> lookupUs;
    lookupUs.reserve(result.lookupCount);

    double lookupTotalUs = 0.0;

    for (const auto id : lookupIds) {
        const auto start = Clock::now();

        auto record = table.get(id);

        const auto end = Clock::now();

        if (!record) {
            throw std::runtime_error(
                "Measured lookup failed."
            );
        }

        checksum +=
            static_cast<std::uint64_t>(record->score);

        const double elapsedUs =
            microseconds(start, end);

        lookupUs.push_back(elapsedUs);
        lookupTotalUs += elapsedUs;
    }

    result.lookupAvgUs =
        result.lookupCount > 0
        ? lookupTotalUs
            / static_cast<double>(result.lookupCount)
        : 0.0;

    result.lookupP50Us =
        percentile(lookupUs, 0.50);

    result.lookupP95Us =
        percentile(lookupUs, 0.95);

    result.lookupP99Us =
        percentile(lookupUs, 0.99);

    const auto scanStart = Clock::now();

    const auto rows = table.scan();

    const auto scanEnd = Clock::now();

    if (rows.size() != recordCount) {
        throw std::runtime_error(
            "Scan returned wrong row count."
        );
    }

    for (const auto& row : rows) {
        checksum +=
            static_cast<std::uint64_t>(row.score);
    }

    result.scanMs =
        milliseconds(scanStart, scanEnd);

    result.scanRps =
        result.scanMs > 0.0
        ? 1000.0
            * static_cast<double>(recordCount)
            / result.scanMs
        : 0.0;

    std::cout
        << "  checksum: "
        << checksum
        << '\n';

    std::filesystem::remove(dbPath);

    return result;
}

void printResults(const Results& r) {
    std::cout
        << "\nDataset: "
        << r.records
        << " records\n"
        << "  insert: "
        << std::fixed
        << std::setprecision(2)
        << r.insertMs
        << " ms ("
        << r.insertRps
        << " records/s)\n"
        << "  lookup count: "
        << r.lookupCount
        << '\n'
        << "  lookup avg: "
        << r.lookupAvgUs
        << " us\n"
        << "  lookup p50/p95/p99: "
        << r.lookupP50Us
        << " / "
        << r.lookupP95Us
        << " / "
        << r.lookupP99Us
        << " us\n"
        << "  full scan: "
        << r.scanMs
        << " ms ("
        << r.scanRps
        << " records/s)\n"
        << "  reopen + index rebuild: "
        << r.reopenMs
        << " ms\n"
        << "  pages: "
        << r.pages
        << '\n'
        << "  file size: "
        << r.fileBytes
        << " bytes\n"
        << "  bytes/record: "
        << r.bytesPerRecord
        << '\n'
        << "  records/page (actual avg): "
        << r.recordsPerPage
        << '\n'
        << "  payload utilization: "
        << r.payloadUtilizationPct
        << "%\n"
        << "  physical used bytes: "
        << r.physicalUsedPct
        << "%\n"
        << "  theoretical max records/full page: "
        << r.theoreticalRecordsPerFullPage
        << '\n'
        << "  theoretical full-page utilization: "
        << r.theoreticalFullPageUtilizationPct
        << "%\n";
}

void writeCsvHeader(std::ofstream& csv) {
    csv
        << "records,"
        << "insert_ms,"
        << "insert_records_per_sec,"
        << "lookup_count,"
        << "lookup_avg_us,"
        << "lookup_p50_us,"
        << "lookup_p95_us,"
        << "lookup_p99_us,"
        << "scan_ms,"
        << "scan_records_per_sec,"
        << "reopen_index_ms,"
        << "pages,"
        << "file_bytes,"
        << "logical_record_bytes,"
        << "bytes_per_record,"
        << "records_per_page,"
        << "payload_utilization_pct,"
        << "physical_used_pct,"
        << "theoretical_records_per_full_page,"
        << "theoretical_full_page_utilization_pct\n";
}

void appendCsv(
    std::ofstream& csv,
    const Results& r
) {
    csv
        << r.records << ','
        << r.insertMs << ','
        << r.insertRps << ','
        << r.lookupCount << ','
        << r.lookupAvgUs << ','
        << r.lookupP50Us << ','
        << r.lookupP95Us << ','
        << r.lookupP99Us << ','
        << r.scanMs << ','
        << r.scanRps << ','
        << r.reopenMs << ','
        << r.pages << ','
        << r.fileBytes << ','
        << r.logicalRecordBytes << ','
        << r.bytesPerRecord << ','
        << r.recordsPerPage << ','
        << r.payloadUtilizationPct << ','
        << r.physicalUsedPct << ','
        << r.theoreticalRecordsPerFullPage << ','
        << r.theoreticalFullPageUtilizationPct
        << '\n';
}

} // namespace

int main(
    int argc,
    char** argv
) {
    std::vector<std::size_t> sizes;

    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            sizes.push_back(
                static_cast<std::size_t>(
                    std::stoull(argv[i])
                )
            );
        }
    }
    else {
        sizes = {1000, 10000, 50000};
    }

    std::ofstream csv(
        "benchmark_results.csv",
        std::ios::trunc
    );

    if (!csv) {
        std::cerr
            << "Could not open benchmark_results.csv\n";

        return 1;
    }

    writeCsvHeader(csv);

    try {
        for (const auto size : sizes) {
            const Results result =
                runBenchmark(size);

            printResults(result);
            appendCsv(csv, result);
            csv.flush();
        }
    }
    catch (const std::exception& error) {
        std::cerr
            << "Benchmark failed: "
            << error.what()
            << '\n';

        return 1;
    }

    std::cout
        << "\nWrote benchmark_results.csv\n";

    return 0;
}
