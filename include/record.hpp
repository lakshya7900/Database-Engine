#pragma once

#include <cstdint>
#include <string>

struct Record {
    std::int32_t id;
    std::string name;
    std::int32_t age;
    std::int32_t score;
};