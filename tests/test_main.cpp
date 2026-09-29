#include "minidb/page.hpp"
#include "minidb/parser.hpp"
#include "minidb/record.hpp"
#include "minidb/table.hpp"
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {
int failures = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "[FAIL] " << message << '\n';
    } else {
        std::cout << "[PASS] " << message << '\n';
    }
}

void testRecordRoundTrip() {
    minidb::Record original{7, "Lakshya Agarwal", 19, 12345678};
    const auto bytes = minidb::serializeRecord(original);
    const auto decoded = minidb::deserializeRecord(bytes);
    expect(decoded.has_value(), "record deserializes");
    expect(decoded && *decoded == original, "record round-trip preserves fields");
}

void testPage() {
    minidb::Page page;
    const char* values[] = {"Hello", "Lakshya", "Agarwal"};
    std::vector<std::uint16_t> slots;

    for (const char* value : values) {
        const auto length = static_cast<std::uint16_t>(std::string(value).size());
        auto slot = page.insertRecord(value, length);
        expect(slot.has_value(), "page insert succeeds");
        if (slot) slots.push_back(*slot);
    }

    expect(page.getSlotCount() == 3, "page slot count is 3");
    expect(page.getFreeSpaceOffset() == 23, "page free-space offset is 23");

    for (std::size_t i = 0; i < slots.size(); ++i) {
        auto bytes = page.getRecord(slots[i]);
        expect(bytes.has_value(), "page getRecord succeeds");
        if (bytes) {
            const std::string value(bytes->begin(), bytes->end());
            expect(value == values[i], "page returned expected bytes");
        }
    }

    expect(page.deleteRecord(slots[1]), "page delete succeeds");
    expect(!page.getRecord(slots[1]).has_value(), "deleted slot is unreadable");

    const char replacement[] = "DB";
    auto reused = page.insertRecord(replacement, 2);
    expect(reused && *reused == slots[1], "deleted slot is reused");
}

void testPersistentTable() {
    const std::filesystem::path path = "minidb_test.db";
    std::filesystem::remove(path);

    {
        minidb::Table table(path.string());
        expect(table.isReady(), "table opens");
        expect(table.insert({1, "Lakshya", 19, 100}), "table insert first record");
        expect(table.insert({2, "Atharv", 20, 10}), "table insert second record");
        expect(!table.insert({1, "Duplicate", 99, 99}), "duplicate id rejected");

        auto record = table.get(2);
        expect(record && record->name == "Atharv", "indexed get returns record");

        expect(table.update(2, {2, "Atharv Updated", 21, 11}), "table update succeeds");
        auto updated = table.get(2);
        expect(updated && updated->name == "Atharv Updated" && updated->age == 21
                   && updated->score == 11,
               "updated record persisted in table");

        expect(table.erase(1), "table delete succeeds");
        expect(!table.get(1).has_value(), "deleted record not found");
    }

    {
        minidb::Table reopened(path.string());
        expect(reopened.isReady(), "table reopens from disk");
        auto record = reopened.get(2);
        expect(record && record->name == "Atharv Updated",
               "index rebuild recovers persistent record");
        expect(reopened.scan().size() == 1, "scan sees one surviving record");

        const auto stats = reopened.stats();
        expect(stats.pageCount >= 1, "stats report at least one page");
        expect(stats.fileBytes == static_cast<std::uint64_t>(stats.pageCount) * minidb::PAGE_SIZE,
               "database file is page aligned");
    }

    std::filesystem::remove(path);
}


void testMultiPagePersistence() {
    const std::filesystem::path path = "minidb_multipage_test.db";
    std::filesystem::remove(path);

    constexpr int recordCount = 250;

    {
        minidb::Table table(path.string());
        expect(table.isReady(), "multi-page table opens");

        for (int i = 0; i < recordCount; ++i) {
            std::string name = "record-" + std::to_string(i) + "-" + std::string(90, 'x');
            if (!table.insert({i, name, 18 + (i % 50), i * 10})) {
                expect(false, "multi-page bulk insert succeeds");
                break;
            }
        }

        const auto stats = table.stats();
        expect(stats.recordCount == recordCount, "multi-page table stores 250 records");
        expect(stats.pageCount > 1, "bulk insert allocates multiple pages");
    }

    {
        minidb::Table reopened(path.string());
        expect(reopened.isReady(), "multi-page table reopens");
        expect(reopened.scan().size() == recordCount, "multi-page startup scan rebuilds full index");

        for (int id : {0, 37, 149, 249}) {
            auto record = reopened.get(id);
            expect(record && record->id == id, "point lookup works across pages");
        }
    }

    std::filesystem::remove(path);
}

void testParser() {
    expect(minidb::parseCommand("INSERT INTO records VALUES (5, 'Alice', 22, 900);").command.has_value(),
           "parser accepts INSERT");
    expect(minidb::parseCommand("SELECT * FROM records WHERE id = 5;").command.has_value(),
           "parser accepts SELECT by id");
    expect(!minidb::parseCommand("THIS IS NOT SQL").command.has_value(),
           "parser rejects invalid command");
}
} // namespace

int main() {
    testRecordRoundTrip();
    testPage();
    testPersistentTable();
    testMultiPagePersistence();
    testParser();

    if (failures != 0) {
        std::cerr << failures << " test(s) failed.\n";
        return 1;
    }

    std::cout << "All MiniDB tests passed.\n";
    return 0;
}
