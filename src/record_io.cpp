#include "record_io.hpp"

#include <cstdint>
#include <cstddef>
#include <optional>

void writeRecord(std::ofstream& file, const Record& record) {
    std::uint32_t nameLength = static_cast<std::uint32_t>(record.name.size());

    file.write(
        reinterpret_cast<const char*>(&record.id),
        sizeof(record.id)
    );

    file.write(
        reinterpret_cast<const char*>(&nameLength),
        sizeof(nameLength)
    );

    std::string name(nameLength, '\0');

    file.write(
        record.name.data(),
        nameLength
    );

    file.write(
        reinterpret_cast<const char*>(&record.age),
        sizeof(record.age)
    );

    file.write(
        reinterpret_cast<const char*>(&record.score),
        sizeof(record.score)
    );
}

Record readRecord(std::ifstream& file) {
    Record record;
    std::uint32_t nameLength = 0;

    file.read(
        reinterpret_cast<char*>(&record.id),
        sizeof(record.id)
    );

    file.read(
        reinterpret_cast<char*>(&nameLength),
        sizeof(nameLength)
    );

    record.name = std::string(nameLength, '\0');

    file.read(
        record.name.data(),
        nameLength
    );

    file.read(
        reinterpret_cast<char*>(&record.age),
        sizeof(record.age)
    );

    file.read(
        reinterpret_cast<char*>(&record.score),
        sizeof(record.score)
    );

    return record;
}

std::size_t recordSize(const Record& record) {
    return sizeof(record.id) + 
        sizeof(std::uint32_t) + 
        record.name.size() + 
        sizeof(record.age) + 
        sizeof(record.score);
}

std::optional<Record> readRecortAt(std::ifstream& file, std::uint32_t recordIndex) {
    file.seekg(0);
    std::uint32_t recordsCount = 0;

    file.read(
        reinterpret_cast<char*>(&recordsCount),
        sizeof(recordsCount)
    );

    if (recordIndex >= recordsCount)
        return std::nullopt;

    std::uint32_t recordOffsetPosition = sizeof(std::uint32_t) + recordIndex * sizeof(std::uint32_t);
    file.seekg(recordOffsetPosition);

    std::uint32_t recordOffset = 0;
    file.read(
        reinterpret_cast<char*>(&recordOffset),
        sizeof(recordOffset)
    );

    file.seekg(recordOffset);
    Record record = readRecord(file);

    return record;
}

void writeRecords(std::ofstream& file, const std::vector<Record>& records) {
    std::vector<std::uint32_t> offsets;

    std::uint32_t recordsCount = static_cast<std::uint32_t>(records.size());
    std::uint32_t headerSize = static_cast<std::uint32_t>(sizeof(std::uint32_t) + records.size() * sizeof(std::uint32_t));
    std::uint32_t currentOffset = static_cast<std::uint32_t>(headerSize);

    for (const Record& record : records) {
        offsets.push_back(currentOffset);
        currentOffset += static_cast<std::uint32_t>(recordSize(record));
    }

    file.write(
        reinterpret_cast<const char*>(&recordsCount),
        sizeof(recordsCount)
    );

    for (std::uint32_t offset : offsets) {
        file.write(
            reinterpret_cast<const char*>(&offset),
            sizeof(offset)
        );
    }

    for (const Record& record : records) {
        writeRecord(file, record);
    }
}