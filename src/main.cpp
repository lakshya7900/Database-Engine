#include <fstream>
#include <iostream>
#include <vector>
#include <optional>

#include "record.hpp"
#include "record_io.hpp"

int main() {
    // WRITE RECORDS
    // std::vector<Record> records = {
    //     {0, "Atharv", 19, 100},
    //     {1, "Sahaj", 20, 10},
    //     {2, "Anirudh", 21, 99},
    //     {3, "Lakshya", 20, 1256},
    //     {4, "Laksh", 23, 12345678}
    // };
    // std::ofstream file("data.db", std::ios::binary);

    // writeRecords(file, records);

    // READ RECORD
    std::uint32_t recordIndex = 100;
    std::ifstream file("data.db", std::ios::binary);
    
    std::optional<Record> record = readRecortAt(file, recordIndex);

    if (record) {
        std::cout << "Id: " << record->id << std::endl;
        std::cout << "Name: " << record->name << std::endl;
        std::cout << "Age: " << record->age << std::endl;
        std::cout << "Score: " << record->score << std::endl;
    } else {
        std::cout << "No record found with index " << recordIndex << std::endl;
    }

    file.close();

    return 0;
}