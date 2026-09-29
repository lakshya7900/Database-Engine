#pragma once
#include "minidb/disk_manager.hpp"
#include "minidb/record.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace minidb {

struct RID {
    std::uint32_t pageId{};
    std::uint16_t slotId{};
};

struct TableStats {
    std::size_t recordCount{};
    std::uint32_t pageCount{};
    std::uint64_t fileBytes{};
    std::size_t logicalRecordBytes{};
};

class Table {
public:
    explicit Table(std::string path);
    bool isReady() const;
    bool insert(const Record& record);
    std::optional<Record> get(std::int32_t id);
    bool erase(std::int32_t id);
    bool update(std::int32_t id, const Record& replacement);
    std::vector<Record> scan();
    TableStats stats();

private:
    DiskManager disk_;
    bool ready_{false};
    std::unordered_map<std::int32_t, RID> index_;

    bool rebuildIndex();
    std::optional<Record> readRID(const RID& rid);
    bool insertInternal(const Record& record, bool enforceUniqueId);
};

} // namespace minidb
