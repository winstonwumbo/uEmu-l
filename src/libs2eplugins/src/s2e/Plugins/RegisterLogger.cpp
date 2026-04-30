#include "RegisterLogger.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sys/time.h>
#include <vector>

namespace s2e {
namespace plugins {
namespace hw {

namespace {
static bool categoryPriority(RegisterLogger::RegisterCategory lhs, RegisterLogger::RegisterCategory rhs) {
    auto score = [](RegisterLogger::RegisterCategory c) {
        switch (c) {
            case RegisterLogger::RegisterCategory::Unknown:
                return 0;
            case RegisterLogger::RegisterCategory::CR:
                return 1;
            case RegisterLogger::RegisterCategory::SR:
                return 1;
            case RegisterLogger::RegisterCategory::DR:
                return 1;
        }
        return 0;
    };

    return score(lhs) < score(rhs);
}

static double getTimestamp() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec + tv.tv_usec / 1e6;
}
}

void RegisterLogger::logRead(uint32_t baseAddress, uint32_t registerAddress, RegisterCategory category) {
    updateEntry(baseAddress, registerAddress, category, true, false);
}

void RegisterLogger::logWrite(uint32_t baseAddress, uint32_t registerAddress, RegisterCategory category) {
    updateEntry(baseAddress, registerAddress, category, false, true);
}

void RegisterLogger::upsert(uint32_t baseAddress, uint32_t registerAddress, bool isRead, bool isWrite,
                            RegisterCategory category) {
    updateEntry(baseAddress, registerAddress, category, isRead, isWrite);
}

void RegisterLogger::updateCategory(uint32_t registerAddress, RegisterCategory category) {
    auto it = m_entries.find(registerAddress);
    if (it != m_entries.end()) {
        it->second.category = mergeCategory(it->second.category, category);
    }
}

std::vector<uint32_t> RegisterLogger::getLoggedAddresses() const {
    std::vector<uint32_t> addresses;
    for (const auto &it : m_entries) {
        addresses.push_back(it.first);
    }
    return addresses;
}

void RegisterLogger::flushToCsv(const std::string &path) const {
    std::ofstream out(path.c_str(), std::ios::out | std::ios::trunc);
    if (!out) {
        return;
    }

    std::vector<std::pair<uint32_t, const RegisterEntry*>> sortedEntries;
    for (const auto &it : m_entries) {
        sortedEntries.emplace_back(it.first, &it.second);
    }

    std::sort(sortedEntries.begin(), sortedEntries.end(),
        [](const std::pair<uint32_t, const RegisterEntry*> &a,
           const std::pair<uint32_t, const RegisterEntry*> &b) {
            if (a.second->baseAddress != b.second->baseAddress) {
                return a.second->baseAddress < b.second->baseAddress;
            }
            return a.second->registerAddress < b.second->registerAddress;
        });

    out << "base_address,register_address,register_category,isread,iswrite,timestamp\n";

    for (const auto &sorted : sortedEntries) {
        const RegisterEntry *entry = sorted.second;

        out << "0x" << std::hex << std::nouppercase << std::setw(8) << std::setfill('0') << entry->baseAddress;
        out << ",";
        out << "0x" << std::hex << std::nouppercase << std::setw(8) << std::setfill('0') << entry->registerAddress;
        out << ",";
        out << categoryToString(entry->category);
        out << ",";
        out << (entry->isRead ? "1" : "0");
        out << ",";
        out << (entry->isWrite ? "1" : "0");
        out << ",";
        out << std::fixed << std::setprecision(6) << entry->timestamp;
        out << "\n";
    }
}

uint32_t RegisterLogger::computeBaseAddress(uint32_t registerAddress) {
    return registerAddress & 0xffffff00;
}

const char *RegisterLogger::categoryToString(RegisterCategory category) {
    switch (category) {
        case RegisterCategory::CR:
            return "CR";
        case RegisterCategory::SR:
            return "SR";
        case RegisterCategory::DR:
            return "DR";
        case RegisterCategory::Unknown:
        default:
            return "UNKNOWN";
    }
}

void RegisterLogger::updateEntry(uint32_t baseAddress, uint32_t registerAddress, RegisterCategory category, bool isRead,
                                 bool isWrite) {
    auto it = m_entries.find(registerAddress);
    if (it == m_entries.end()) {
        RegisterEntry entry;
        entry.baseAddress = baseAddress;
        entry.registerAddress = registerAddress;
        entry.category = category;
        entry.isRead = isRead;
        entry.isWrite = isWrite;
        entry.timestamp = getTimestamp();
        m_entries.emplace(registerAddress, entry);
        return;
    }

    RegisterEntry &entry = it->second;
    entry.baseAddress = baseAddress;
    entry.category = mergeCategory(entry.category, category);
    entry.isRead = entry.isRead || isRead;
    entry.isWrite = entry.isWrite || isWrite;
    entry.timestamp = getTimestamp();
}

RegisterLogger::RegisterCategory RegisterLogger::mergeCategory(RegisterCategory oldCategory, RegisterCategory newCategory) {
    if (oldCategory == newCategory) {
        return oldCategory;
    }

    if (oldCategory == RegisterCategory::Unknown) {
        return newCategory;
    }

    if (newCategory == RegisterCategory::Unknown) {
        return oldCategory;
    }

    if (oldCategory == RegisterCategory::CR && (newCategory == RegisterCategory::SR || newCategory == RegisterCategory::DR)) {
        return newCategory;
    }

    if (newCategory == RegisterCategory::CR && (oldCategory == RegisterCategory::SR || oldCategory == RegisterCategory::DR)) {
        return oldCategory;
    }

    if (oldCategory == RegisterCategory::SR && newCategory == RegisterCategory::DR) {
        return RegisterCategory::DR;
    }

    if (oldCategory == RegisterCategory::DR && newCategory == RegisterCategory::SR) {
        return RegisterCategory::DR;
    }

    if (categoryPriority(oldCategory, newCategory)) {
        return newCategory;
    }

    return oldCategory;
}

} // namespace hw
} // namespace plugins
} // namespace s2e
