#pragma once

#include <fstream>
#include <cstddef>
#include <vector>
#include <optional>

#include "record.hpp"

std::size_t recordSize(const Record& record);

void writeRecord(std::ofstream& file, const Record& record);
Record readRecord(std::ifstream& file);

std::optional<Record> readRecortAt(std::ifstream& file, std::uint32_t recordIndex);
void writeRecords(std::ofstream& file, const std::vector<Record>& records);