#include "minidb/parser.hpp"
#include "minidb/table.hpp"
#include <iomanip>
#include <iostream>
#include <string>
#include <type_traits>
#include <variant>

namespace {

void printRecord(const minidb::Record& record) {
    std::cout << std::left
              << std::setw(8) << record.id
              << std::setw(24) << record.name
              << std::setw(8) << record.age
              << record.score << '\n';
}

void printHeader() {
    std::cout << std::left
              << std::setw(8) << "id"
              << std::setw(24) << "name"
              << std::setw(8) << "age"
              << "score\n"
              << std::string(52, '-') << '\n';
}

} // namespace

int main(int argc, char** argv) {
    std::string dbPath = "data/minidb.db";
    if (argc >= 2) dbPath = argv[1];

    minidb::Table table(dbPath);
    if (!table.isReady()) {
        std::cerr << "Failed to open or recover database: " << dbPath << '\n';
        return 1;
    }

    std::cout << "MiniDB ready: " << dbPath << "\nType .help for commands.\n";
    std::string line;

    while (true) {
        std::cout << "minidb> ";
        if (!std::getline(std::cin, line)) break;

        const auto parsed = minidb::parseCommand(line);
        if (!parsed.command) {
            std::cout << "Error: " << parsed.error << '\n';
            continue;
        }

        bool shouldExit = false;

        std::visit([&](const auto& command) {
            using T = std::decay_t<decltype(command)>;

            if constexpr (std::is_same_v<T, minidb::InsertCommand>) {
                std::cout << (table.insert(command.record) ? "Inserted.\n"
                    : "Insert failed (duplicate id, oversized record, or I/O error).\n");
            } else if constexpr (std::is_same_v<T, minidb::SelectAllCommand>) {
                const auto records = table.scan();
                printHeader();
                for (const auto& record : records) printRecord(record);
                std::cout << records.size() << " row(s).\n";
            } else if constexpr (std::is_same_v<T, minidb::SelectByIdCommand>) {
                auto record = table.get(command.id);
                if (!record) {
                    std::cout << "No record with id " << command.id << ".\n";
                    return;
                }
                printHeader();
                printRecord(*record);
            } else if constexpr (std::is_same_v<T, minidb::DeleteCommand>) {
                std::cout << (table.erase(command.id) ? "Deleted.\n"
                    : "No record with that id.\n");
            } else if constexpr (std::is_same_v<T, minidb::UpdateCommand>) {
                std::cout << (table.update(command.id, command.replacement) ? "Updated.\n"
                    : "Update failed.\n");
            } else if constexpr (std::is_same_v<T, minidb::StatsCommand>) {
                const auto stats = table.stats();
                std::cout << "records: " << stats.recordCount << '\n'
                          << "pages: " << stats.pageCount << '\n'
                          << "page_size: " << minidb::PAGE_SIZE << " bytes\n"
                          << "file_size: " << stats.fileBytes << " bytes\n"
                          << "logical_record_bytes: " << stats.logicalRecordBytes << " bytes\n";
            } else if constexpr (std::is_same_v<T, minidb::HelpCommand>) {
                std::cout << minidb::helpText();
            } else if constexpr (std::is_same_v<T, minidb::ExitCommand>) {
                shouldExit = true;
            }
        }, *parsed.command);

        if (shouldExit) break;
    }
    return 0;
}
