#pragma once
#include "minidb/record.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace minidb {

struct InsertCommand { Record record; };
struct SelectAllCommand {};
struct SelectByIdCommand { std::int32_t id{}; };
struct DeleteCommand { std::int32_t id{}; };
struct UpdateCommand { std::int32_t id{}; Record replacement; };
struct StatsCommand {};
struct HelpCommand {};
struct ExitCommand {};

using Command = std::variant<InsertCommand, SelectAllCommand, SelectByIdCommand,
                             DeleteCommand, UpdateCommand, StatsCommand,
                             HelpCommand, ExitCommand>;

struct ParseResult {
    std::optional<Command> command;
    std::string error;
};

ParseResult parseCommand(const std::string& input);
std::string helpText();

} // namespace minidb
