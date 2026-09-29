#pragma once
#include "minidb/page.hpp"
#include <cstdint>
#include <fstream>
#include <optional>
#include <string>

namespace minidb {

class DiskManager {
public:
    explicit DiskManager(std::string path);
    ~DiskManager();

    DiskManager(const DiskManager&) = delete;
    DiskManager& operator=(const DiskManager&) = delete;

    bool isOpen() const;
    std::uint32_t pageCount();
    bool readPage(std::uint32_t pageId, Page& page);
    bool writePage(std::uint32_t pageId, const Page& page);
    std::optional<std::uint32_t> allocatePage();
    bool flush();
    std::uint64_t fileSize();

private:
    std::string path_;
    std::fstream file_;

    bool ensureOpen();
    std::streamoff pageOffset(std::uint32_t pageId) const;
};

} // namespace minidb
