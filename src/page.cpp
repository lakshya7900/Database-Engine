#include "minidb/page.hpp"
#include <cstring>
#include <limits>

namespace minidb {

Page::Page() { initialize(); }

void Page::initialize() {
    data_.fill(0);
    setSlotCount(0);
    setFreeSpaceOffset(static_cast<std::uint16_t>(PAGE_HEADER_SIZE));
}

bool Page::load(const char* bytes, std::size_t size) {
    if (bytes == nullptr || size != PAGE_SIZE) return false;
    std::memcpy(data_.data(), bytes, PAGE_SIZE);

    const auto slotCount = getSlotCount();
    const auto freeOffset = getFreeSpaceOffset();
    if (freeOffset < PAGE_HEADER_SIZE || freeOffset > PAGE_SIZE) return false;

    const std::size_t directoryStart = PAGE_SIZE - static_cast<std::size_t>(slotCount) * SLOT_SIZE;
    if (freeOffset > directoryStart) return false;

    for (std::uint16_t slotId = 0; slotId < slotCount; ++slotId) {
        const Slot slot = readSlot(slotId);
        if (slot.length == 0) continue;
        const std::size_t end = static_cast<std::size_t>(slot.offset) + slot.length;
        if (slot.offset < PAGE_HEADER_SIZE || end > directoryStart) return false;
    }
    return true;
}

const char* Page::data() const { return data_.data(); }

std::uint16_t Page::getSlotCount() const {
    std::uint16_t value = 0;
    std::memcpy(&value, data_.data(), sizeof(value));
    return value;
}

std::uint16_t Page::getFreeSpaceOffset() const {
    std::uint16_t value = 0;
    std::memcpy(&value, data_.data() + sizeof(std::uint16_t), sizeof(value));
    return value;
}

void Page::setSlotCount(std::uint16_t value) {
    std::memcpy(data_.data(), &value, sizeof(value));
}

void Page::setFreeSpaceOffset(std::uint16_t value) {
    std::memcpy(data_.data() + sizeof(std::uint16_t), &value, sizeof(value));
}

std::size_t Page::slotPosition(std::uint16_t slotId) const {
    return PAGE_SIZE - (static_cast<std::size_t>(slotId) + 1) * SLOT_SIZE;
}

Page::Slot Page::readSlot(std::uint16_t slotId) const {
    Slot slot{};
    const std::size_t position = slotPosition(slotId);
    std::memcpy(&slot.offset, data_.data() + position, sizeof(slot.offset));
    std::memcpy(&slot.length, data_.data() + position + sizeof(std::uint16_t), sizeof(slot.length));
    return slot;
}

void Page::writeSlot(std::uint16_t slotId, const Slot& slot) {
    const std::size_t position = slotPosition(slotId);
    std::memcpy(data_.data() + position, &slot.offset, sizeof(slot.offset));
    std::memcpy(data_.data() + position + sizeof(std::uint16_t), &slot.length, sizeof(slot.length));
}

std::optional<std::uint16_t> Page::findReusableSlot() const {
    const auto slotCount = getSlotCount();
    for (std::uint16_t slotId = 0; slotId < slotCount; ++slotId) {
        if (readSlot(slotId).length == 0) return slotId;
    }
    return std::nullopt;
}

std::size_t Page::freeSpace() const {
    const auto slotCount = getSlotCount();
    const auto freeOffset = getFreeSpaceOffset();
    const std::size_t directoryStart = PAGE_SIZE - static_cast<std::size_t>(slotCount) * SLOT_SIZE;
    if (freeOffset > directoryStart) return 0;
    return directoryStart - freeOffset;
}

void Page::compact() {
    const std::uint16_t slotCount = getSlotCount();
    const std::array<char, PAGE_SIZE> oldData = data_;

    data_.fill(0);
    setSlotCount(slotCount);
    std::uint16_t nextOffset = static_cast<std::uint16_t>(PAGE_HEADER_SIZE);

    for (std::uint16_t slotId = 0; slotId < slotCount; ++slotId) {
        const std::size_t oldSlotPos = PAGE_SIZE - (static_cast<std::size_t>(slotId) + 1) * SLOT_SIZE;
        Slot slot{};
        std::memcpy(&slot.offset, oldData.data() + oldSlotPos, sizeof(slot.offset));
        std::memcpy(&slot.length, oldData.data() + oldSlotPos + sizeof(std::uint16_t), sizeof(slot.length));

        if (slot.length == 0) {
            writeSlot(slotId, Slot{0, 0});
            continue;
        }

        std::memcpy(data_.data() + nextOffset, oldData.data() + slot.offset, slot.length);
        slot.offset = nextOffset;
        writeSlot(slotId, slot);
        nextOffset = static_cast<std::uint16_t>(nextOffset + slot.length);
    }

    setFreeSpaceOffset(nextOffset);
}

std::optional<std::uint16_t> Page::insertRecord(const char* recordData, std::uint16_t recordLength) {
    if (recordData == nullptr || recordLength == 0) return std::nullopt;

    auto reusableSlot = findReusableSlot();
    std::size_t requiredSpace = static_cast<std::size_t>(recordLength);
    if (!reusableSlot) requiredSpace += SLOT_SIZE;

    if (requiredSpace > freeSpace()) {
        compact();
        if (requiredSpace > freeSpace()) return std::nullopt;
    }

    std::uint16_t slotId = 0;
    if (reusableSlot) {
        slotId = *reusableSlot;
    } else {
        const std::uint16_t slotCount = getSlotCount();
        if (slotCount == std::numeric_limits<std::uint16_t>::max()) return std::nullopt;
        slotId = slotCount;
        setSlotCount(static_cast<std::uint16_t>(slotCount + 1));
    }

    const std::uint16_t offset = getFreeSpaceOffset();
    std::memcpy(data_.data() + offset, recordData, recordLength);
    writeSlot(slotId, Slot{offset, recordLength});
    setFreeSpaceOffset(static_cast<std::uint16_t>(offset + recordLength));
    return slotId;
}

std::optional<std::vector<char>> Page::getRecord(std::uint16_t slotId) const {
    if (slotId >= getSlotCount()) return std::nullopt;
    const Slot slot = readSlot(slotId);
    if (slot.length == 0) return std::nullopt;

    std::vector<char> bytes(slot.length);
    std::memcpy(bytes.data(), data_.data() + slot.offset, slot.length);
    return bytes;
}

bool Page::deleteRecord(std::uint16_t slotId) {
    if (slotId >= getSlotCount()) return false;
    const Slot slot = readSlot(slotId);
    if (slot.length == 0) return false;
    writeSlot(slotId, Slot{0, 0});
    compact();
    return true;
}

bool Page::isSlotActive(std::uint16_t slotId) const {
    return slotId < getSlotCount() && readSlot(slotId).length != 0;
}

std::vector<std::uint16_t> Page::activeSlots() const {
    std::vector<std::uint16_t> slots;
    const auto slotCount = getSlotCount();
    slots.reserve(slotCount);
    for (std::uint16_t slotId = 0; slotId < slotCount; ++slotId) {
        if (isSlotActive(slotId)) slots.push_back(slotId);
    }
    return slots;
}

} // namespace minidb
