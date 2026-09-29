#include "minidb/table.hpp"
#include <algorithm>
#include <limits>
#include <utility>

namespace minidb {

Table::Table(std::string path) : disk_(std::move(path)) {
    ready_ = disk_.isOpen()
        && (disk_.fileSize() % PAGE_SIZE == 0)
        && rebuildIndex();
}

bool Table::isReady() const { return ready_; }

bool Table::rebuildIndex() {
    index_.clear();
    const std::uint32_t pages = disk_.pageCount();

    for (std::uint32_t pageId = 0; pageId < pages; ++pageId) {
        Page page;
        if (!disk_.readPage(pageId, page)) return false;

        for (std::uint16_t slotId : page.activeSlots()) {
            auto bytes = page.getRecord(slotId);
            if (!bytes) return false;
            auto record = deserializeRecord(*bytes);
            if (!record || index_.contains(record->id)) return false;
            index_[record->id] = RID{pageId, slotId};
        }
    }
    return true;
}

std::optional<Record> Table::readRID(const RID& rid) {
    Page page;
    if (!disk_.readPage(rid.pageId, page)) return std::nullopt;
    auto bytes = page.getRecord(rid.slotId);
    if (!bytes) return std::nullopt;
    return deserializeRecord(*bytes);
}

bool Table::insertInternal(const Record& record, bool enforceUniqueId) {
    if (!ready_) return false;
    if (enforceUniqueId && index_.contains(record.id)) return false;

    std::vector<char> bytes = serializeRecord(record);
    if (bytes.empty() || bytes.size() > std::numeric_limits<std::uint16_t>::max()) return false;
    if (bytes.size() + SLOT_SIZE > PAGE_SIZE - PAGE_HEADER_SIZE) return false;
    const auto length = static_cast<std::uint16_t>(bytes.size());

    const std::uint32_t pages = disk_.pageCount();
    for (std::uint32_t pageId = 0; pageId < pages; ++pageId) {
        Page page;
        if (!disk_.readPage(pageId, page)) return false;
        auto slot = page.insertRecord(bytes.data(), length);
        if (!slot) continue;
        if (!disk_.writePage(pageId, page)) return false;
        index_[record.id] = RID{pageId, *slot};
        return disk_.flush();
    }

    auto newPageId = disk_.allocatePage();
    if (!newPageId) return false;
    Page page;
    if (!disk_.readPage(*newPageId, page)) return false;
    auto slot = page.insertRecord(bytes.data(), length);
    if (!slot) return false;
    if (!disk_.writePage(*newPageId, page)) return false;
    index_[record.id] = RID{*newPageId, *slot};
    return disk_.flush();
}

bool Table::insert(const Record& record) { return insertInternal(record, true); }

std::optional<Record> Table::get(std::int32_t id) {
    const auto it = index_.find(id);
    if (it == index_.end()) return std::nullopt;
    return readRID(it->second);
}

bool Table::erase(std::int32_t id) {
    const auto it = index_.find(id);
    if (it == index_.end()) return false;

    const RID rid = it->second;
    Page page;
    if (!disk_.readPage(rid.pageId, page)) return false;
    if (!page.deleteRecord(rid.slotId)) return false;
    if (!disk_.writePage(rid.pageId, page)) return false;
    index_.erase(it);
    return disk_.flush();
}

bool Table::update(std::int32_t id, const Record& replacement) {
    auto oldRecord = get(id);
    if (!oldRecord) return false;
    if (replacement.id != id && index_.contains(replacement.id)) return false;

    if (!erase(id)) return false;
    if (insertInternal(replacement, replacement.id != id)) return true;

    insertInternal(*oldRecord, false);
    return false;
}

std::vector<Record> Table::scan() {
    std::vector<Record> records;
    records.reserve(index_.size());

    const std::uint32_t pages = disk_.pageCount();
    for (std::uint32_t pageId = 0; pageId < pages; ++pageId) {
        Page page;
        if (!disk_.readPage(pageId, page)) continue;

        for (std::uint16_t slotId : page.activeSlots()) {
            auto bytes = page.getRecord(slotId);
            if (!bytes) continue;
            auto record = deserializeRecord(*bytes);
            if (record) records.push_back(std::move(*record));
        }
    }

    std::sort(records.begin(), records.end(), [](const Record& a, const Record& b) {
        return a.id < b.id;
    });
    return records;
}

TableStats Table::stats() {
    TableStats result;
    result.recordCount = index_.size();
    result.pageCount = disk_.pageCount();
    result.fileBytes = disk_.fileSize();
    for (const auto& record : scan()) result.logicalRecordBytes += serializedRecordSize(record);
    return result;
}

} // namespace minidb
