#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace minidb {

struct Record {
    std::int32_t id{};
    std::string name;
    std::int32_t age{};
    std::int32_t score{};
    bool operator==(const Record&) const = default;
};

std::size_t serializedRecordSize(const Record& record);
std::vector<char> serializeRecord(const Record& record);
std::optional<Record> deserializeRecord(const char* data, std::size_t size);

inline std::optional<Record> deserializeRecord(const std::vector<char>& bytes) {
    return deserializeRecord(bytes.data(), bytes.size());
}

} // namespace minidb
