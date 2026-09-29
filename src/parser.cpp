#include "minidb/parser.hpp"
#include <algorithm>
#include <cctype>
#include <limits>
#include <regex>
#include <string>
#include <utility>

namespace minidb {
namespace {

std::string trim(const std::string& input) {
    const auto first = std::find_if_not(input.begin(), input.end(), [](unsigned char ch) {
        return std::isspace(ch);
    });
    if (first == input.end()) return {};

    const auto last = std::find_if_not(input.rbegin(), input.rend(), [](unsigned char ch) {
        return std::isspace(ch);
    }).base();
    return std::string(first, last);
}

bool parseInt32(const std::string& text, std::int32_t& value) {
    try {
        std::size_t consumed = 0;
        const long long parsed = std::stoll(text, &consumed);
        if (consumed != text.size()) return false;
        if (parsed < std::numeric_limits<std::int32_t>::min()
            || parsed > std::numeric_limits<std::int32_t>::max()) return false;
        value = static_cast<std::int32_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

ParseResult parseCommand(const std::string& rawInput) {
    const std::string input = trim(rawInput);
    if (input.empty()) return {std::nullopt, "Empty command."};

    if (input == ".exit" || input == ".quit") return {Command{ExitCommand{}}, {}};
    if (input == ".help") return {Command{HelpCommand{}}, {}};
    if (input == ".stats") return {Command{StatsCommand{}}, {}};

    static const std::regex insertPattern(
        R"(^\s*INSERT\s+INTO\s+records\s+VALUES\s*\(\s*(-?\d+)\s*,\s*'([^']*)'\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*\)\s*;?\s*$)",
        std::regex::icase);
    static const std::regex selectAllPattern(
        R"(^\s*SELECT\s+\*\s+FROM\s+records\s*;?\s*$)", std::regex::icase);
    static const std::regex selectByIdPattern(
        R"(^\s*SELECT\s+\*\s+FROM\s+records\s+WHERE\s+id\s*=\s*(-?\d+)\s*;?\s*$)",
        std::regex::icase);
    static const std::regex deletePattern(
        R"(^\s*DELETE\s+FROM\s+records\s+WHERE\s+id\s*=\s*(-?\d+)\s*;?\s*$)",
        std::regex::icase);
    static const std::regex updatePattern(
        R"(^\s*UPDATE\s+records\s+SET\s+name\s*=\s*'([^']*)'\s*,\s*age\s*=\s*(-?\d+)\s*,\s*score\s*=\s*(-?\d+)\s+WHERE\s+id\s*=\s*(-?\d+)\s*;?\s*$)",
        std::regex::icase);

    std::smatch match;

    if (std::regex_match(input, match, insertPattern)) {
        Record record;
        if (!parseInt32(match[1].str(), record.id)
            || !parseInt32(match[3].str(), record.age)
            || !parseInt32(match[4].str(), record.score)) {
            return {std::nullopt, "Integer is outside int32 range."};
        }
        record.name = match[2].str();
        return {Command{InsertCommand{std::move(record)}}, {}};
    }

    if (std::regex_match(input, selectAllPattern)) return {Command{SelectAllCommand{}}, {}};

    if (std::regex_match(input, match, selectByIdPattern)) {
        std::int32_t id = 0;
        if (!parseInt32(match[1].str(), id)) return {std::nullopt, "Invalid id."};
        return {Command{SelectByIdCommand{id}}, {}};
    }

    if (std::regex_match(input, match, deletePattern)) {
        std::int32_t id = 0;
        if (!parseInt32(match[1].str(), id)) return {std::nullopt, "Invalid id."};
        return {Command{DeleteCommand{id}}, {}};
    }

    if (std::regex_match(input, match, updatePattern)) {
        std::int32_t id = 0;
        Record replacement;
        replacement.name = match[1].str();
        if (!parseInt32(match[2].str(), replacement.age)
            || !parseInt32(match[3].str(), replacement.score)
            || !parseInt32(match[4].str(), id)) {
            return {std::nullopt, "Invalid integer in UPDATE."};
        }
        replacement.id = id;
        return {Command{UpdateCommand{id, std::move(replacement)}}, {}};
    }

    return {std::nullopt, "Unrecognized command. Type .help"};
}

std::string helpText() {
    return
        "Commands:\n"
        "  INSERT INTO records VALUES (1, 'Lakshya', 19, 100);\n"
        "  SELECT * FROM records;\n"
        "  SELECT * FROM records WHERE id = 1;\n"
        "  UPDATE records SET name = 'New Name', age = 20, score = 200 WHERE id = 1;\n"
        "  DELETE FROM records WHERE id = 1;\n"
        "  .stats\n"
        "  .help\n"
        "  .exit\n";
}

} // namespace minidb
