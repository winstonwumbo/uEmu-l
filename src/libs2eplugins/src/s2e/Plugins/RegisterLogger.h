#ifndef S2E_PLUGINS_uEmu_RegisterLogger_H
#define S2E_PLUGINS_uEmu_RegisterLogger_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace s2e {
namespace plugins {
namespace hw {

class RegisterLogger {
public:
    enum class RegisterCategory {
        Unknown,
        CR,
        SR,
        DR,
    };

    void logRead(uint32_t baseAddress, uint32_t registerAddress, RegisterCategory category);
    void logWrite(uint32_t baseAddress, uint32_t registerAddress, RegisterCategory category);
    void upsert(uint32_t baseAddress, uint32_t registerAddress, bool isRead, bool isWrite, RegisterCategory category);
    void updateCategory(uint32_t registerAddress, RegisterCategory category);
    std::vector<uint32_t> getLoggedAddresses() const;
    void flushToCsv(const std::string &path) const;

    static uint32_t computeBaseAddress(uint32_t registerAddress);
    static const char *categoryToString(RegisterCategory category);

private:
    struct RegisterEntry {
        uint32_t baseAddress;
        uint32_t registerAddress;
        RegisterCategory category;
        bool isRead;
        bool isWrite;
        double timestamp;
    };

    std::map<uint32_t, RegisterEntry> m_entries;

    void updateEntry(uint32_t baseAddress, uint32_t registerAddress, RegisterCategory category, bool isRead, bool isWrite);
    static RegisterCategory mergeCategory(RegisterCategory oldCategory, RegisterCategory newCategory);
};

} // namespace hw
} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_uEmu_RegisterLogger_H
