///
/// Copyright (C) 2026
///
/// Licensed under the Cyberhaven Research License Agreement.
///

#ifndef S2E_PLUGINS_uEmu_RegisterLogPlotter_H
#define S2E_PLUGINS_uEmu_RegisterLogPlotter_H

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace s2e {
namespace plugins {
namespace hw {

class RegisterLogPlotter {
public:
    struct Options {
        std::string inputCsv;
        std::string outputDir;
        std::string outputPrefix;
        unsigned topNBaseAddresses;
    };

    bool generate(const Options &options, std::vector<std::string> &generatedPlots, std::string &error) const;

private:
    struct CsvEntry {
        uint32_t baseAddress;
        uint32_t registerAddress;
        std::string category;
        bool isRead;
        bool isWrite;
        double timestamp;
    };

    bool loadCsv(const std::string &csvPath, std::vector<CsvEntry> &entries, std::string &error) const;
    std::string makeOutputPath(const Options &options, const std::string &suffix) const;

    static bool parseHex32(const std::string &value, uint32_t &result);
    static bool parseBoolFlag(const std::string &value, bool &result);
    static std::vector<std::string> splitCsvLine(const std::string &line);
    static std::string trim(const std::string &value);
    static std::string toLower(std::string value);
    static std::string categoryKey(const std::string &category);

    void plotCategoryCounts(const std::map<std::string, uint64_t> &counts, const Options &options,
                            std::vector<std::string> &generatedPlots) const;
    void plotTopBaseAddresses(const std::map<uint32_t, uint64_t> &baseCounts, const Options &options,
                              std::vector<std::string> &generatedPlots) const;
    void plotReadWriteMix(const std::map<std::string, std::pair<uint64_t, uint64_t>> &rwByCategory,
                          const Options &options, std::vector<std::string> &generatedPlots) const;
    void plotRegisterTimeline(const std::vector<CsvEntry> &entries, const Options &options,
                               std::vector<std::string> &generatedPlots) const;
};

} // namespace hw
} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_uEmu_RegisterLogPlotter_H
