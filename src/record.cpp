#include "minidb/record.hpp"
#include <cstring>
#include <limits>

namespace minidb {
namespace {

template <typename T>
void appendValue(std::vector<char>& output, const T& value) {
    const char* begin = reinterpret_cast<const char*>(&value);
    output.insert(output.end(), begin, begin + sizeof(T));
}

template <typename T>
bool readValue(const char*& cursor, std::size_t& remaining, T& value) {
    if (remaining < sizeof(T)) return false;
    std::memcpy(&value, cursor, sizeof(T));
    cursor += sizeof(T);
    remaining -= sizeof(T);
    return true;
}

} // namespace

std::size_t serializedRecordSize(const Record& record) {
    return sizeof(record.id) + sizeof(std::uint32_t) + record.name.size()
        + sizeof(record.age) + sizeof(record.score);
}

std::vector<char> serializeRecord(const Record& record) {
    if (record.name.size() > std::numeric_limits<std::uint32_t>::max()) return {};

    std::vector<char> output;
    output.reserve(serializedRecordSize(record));
    const std::uint32_t nameLength = static_cast<std::uint32_t>(record.name.size());

    appendValue(output, record.id);
    appendValue(output, nameLength);
    output.insert(output.end(), record.name.begin(), record.name.end());
    appendValue(output, record.age);
    appendValue(output, record.score);
    return output;
}

std::optional<Record> deserializeRecord(const char* data, std::size_t size) {
    if (data == nullptr) return std::nullopt;

    const char* cursor = data;
    std::size_t remaining = size;
    Record record;
    std::uint32_t nameLength = 0;

    if (!readValue(cursor, remaining, record.id)) return std::nullopt;
    if (!readValue(cursor, remaining, nameLength)) return std::nullopt;
    if (remaining < nameLength) return std::nullopt;

    record.name.assign(cursor, cursor + nameLength);
    cursor += nameLength;
    remaining -= nameLength;

    if (!readValue(cursor, remaining, record.age)) return std::nullopt;
    if (!readValue(cursor, remaining, record.score)) return std::nullopt;
    if (remaining != 0) return std::nullopt;

    return record;
}

} // namespace minidb
