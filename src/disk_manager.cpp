#include "minidb/disk_manager.hpp"
#include <array>
#include <filesystem>
#include <utility>

namespace minidb {

DiskManager::DiskManager(std::string path) : path_(std::move(path)) {
    ensureOpen();
}

DiskManager::~DiskManager() {
    if (file_.is_open()) {
        flush();
        file_.close();
    }
}

bool DiskManager::ensureOpen() {
    if (file_.is_open()) return true;

    const std::filesystem::path dbPath(path_);
    if (dbPath.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(dbPath.parent_path(), ec);
    }

    file_.open(path_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file_) {
        file_.clear();
        std::ofstream create(path_, std::ios::binary);
        if (!create) return false;
        create.close();
        file_.open(path_, std::ios::in | std::ios::out | std::ios::binary);
    }
    return static_cast<bool>(file_);
}

bool DiskManager::isOpen() const { return file_.is_open(); }

std::streamoff DiskManager::pageOffset(std::uint32_t pageId) const {
    return static_cast<std::streamoff>(pageId) * static_cast<std::streamoff>(PAGE_SIZE);
}

std::uint64_t DiskManager::fileSize() {
    if (!ensureOpen()) return 0;
    file_.clear();
    file_.seekg(0, std::ios::end);
    const auto end = file_.tellg();
    if (end < 0) return 0;
    return static_cast<std::uint64_t>(end);
}

std::uint32_t DiskManager::pageCount() {
    return static_cast<std::uint32_t>(fileSize() / PAGE_SIZE);
}

bool DiskManager::readPage(std::uint32_t pageId, Page& page) {
    if (!ensureOpen() || pageId >= pageCount()) return false;

    std::array<char, PAGE_SIZE> buffer{};
    file_.clear();
    file_.seekg(pageOffset(pageId));
    file_.read(buffer.data(), static_cast<std::streamsize>(PAGE_SIZE));

    if (file_.gcount() != static_cast<std::streamsize>(PAGE_SIZE)) return false;
    return page.load(buffer.data(), buffer.size());
}

bool DiskManager::writePage(std::uint32_t pageId, const Page& page) {
    if (!ensureOpen() || pageId >= pageCount()) return false;
    file_.clear();
    file_.seekp(pageOffset(pageId));
    file_.write(page.data(), static_cast<std::streamsize>(PAGE_SIZE));
    return static_cast<bool>(file_);
}

std::optional<std::uint32_t> DiskManager::allocatePage() {
    if (!ensureOpen()) return std::nullopt;

    const std::uint32_t newPageId = pageCount();
    Page page;
    file_.clear();
    file_.seekp(0, std::ios::end);
    file_.write(page.data(), static_cast<std::streamsize>(PAGE_SIZE));
    if (!file_) return std::nullopt;
    file_.flush();
    return newPageId;
}

bool DiskManager::flush() {
    if (!file_.is_open()) return false;
    file_.flush();
    return static_cast<bool>(file_);
}

} // namespace minidb
