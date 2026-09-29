#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace minidb {

constexpr std::size_t PAGE_SIZE = 4096;
constexpr std::size_t PAGE_HEADER_SIZE = sizeof(std::uint16_t) * 2;
constexpr std::size_t SLOT_SIZE = sizeof(std::uint16_t) * 2;

class Page {
public:
    Page();
    void initialize();
    bool load(const char* bytes, std::size_t size);
    const char* data() const;

    std::uint16_t getSlotCount() const;
    std::uint16_t getFreeSpaceOffset() const;
    std::size_t freeSpace() const;

    std::optional<std::uint16_t> insertRecord(const char* recordData, std::uint16_t recordLength);
    std::optional<std::vector<char>> getRecord(std::uint16_t slotId) const;
    bool deleteRecord(std::uint16_t slotId);
    bool isSlotActive(std::uint16_t slotId) const;
    std::vector<std::uint16_t> activeSlots() const;

private:
    struct Slot {
        std::uint16_t offset{};
        std::uint16_t length{};
    };

    std::array<char, PAGE_SIZE> data_{};

    void setSlotCount(std::uint16_t value);
    void setFreeSpaceOffset(std::uint16_t value);
    std::size_t slotPosition(std::uint16_t slotId) const;
    Slot readSlot(std::uint16_t slotId) const;
    void writeSlot(std::uint16_t slotId, const Slot& slot);
    std::optional<std::uint16_t> findReusableSlot() const;
    void compact();
};

} // namespace minidb
