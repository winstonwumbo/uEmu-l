///
/// Copyright (C) 2026
///
/// Licensed under the Cyberhaven Research License Agreement.
///

#include "RegisterLogPlotter.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

#include <matplot/matplot.h>

namespace s2e {
namespace plugins {
namespace hw {

bool RegisterLogPlotter::generate(const Options &options, std::vector<std::string> &generatedPlots,
                                  std::string &error) const {
    generatedPlots.clear();
    std::vector<CsvEntry> entries;
    if (!loadCsv(options.inputCsv, entries, error)) {
        return false;
    }

    std::map<std::string, uint64_t> categoryCounts;
    std::map<uint32_t, uint64_t> baseCounts;
    std::map<std::string, std::pair<uint64_t, uint64_t>> rwByCategory;

    for (const auto &entry : entries) {
        std::string key = categoryKey(entry.category);
        categoryCounts[key]++;
        baseCounts[entry.baseAddress]++;
        if (entry.isRead) {
            rwByCategory[key].first++;
        }
        if (entry.isWrite) {
            rwByCategory[key].second++;
        }
    }

    try {
        plotCategoryCounts(categoryCounts, options, generatedPlots);
        plotTopBaseAddresses(baseCounts, options, generatedPlots);
        plotReadWriteMix(rwByCategory, options, generatedPlots);
        plotRegisterTimeline(entries, options, generatedPlots);
    } catch (const std::exception &e) {
        error = std::string("failed to generate plots: ") + e.what();
        return false;
    } catch (...) {
        error = "failed to generate plots with unknown error";
        return false;
    }

    return true;
}

bool RegisterLogPlotter::loadCsv(const std::string &csvPath, std::vector<CsvEntry> &entries, std::string &error) const {
    std::ifstream in(csvPath.c_str(), std::ios::in);
    if (!in) {
        error = std::string("could not open CSV: ") + csvPath;
        return false;
    }

    std::string line;
    if (!std::getline(in, line)) {
        error = std::string("CSV is empty: ") + csvPath;
        return false;
    }

    uint64_t malformed = 0;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }

        auto cols = splitCsvLine(line);
        if (cols.size() != 6) {
            malformed++;
            continue;
        }

        CsvEntry entry;
        entry.category = trim(cols[2]);
        if (!parseHex32(cols[0], entry.baseAddress) || !parseHex32(cols[1], entry.registerAddress) ||
            !parseBoolFlag(cols[3], entry.isRead) || !parseBoolFlag(cols[4], entry.isWrite)) {
            malformed++;
            continue;
        }

        try {
            entry.timestamp = std::stod(trim(cols[5]));
        } catch (...) {
            malformed++;
            continue;
        }

        entries.push_back(entry);
    }

    if (entries.empty()) {
        error = std::string("parsed 0 entries from: ") + csvPath;
        return false;
    }

    if (malformed > 0) {
        std::ostringstream ss;
        ss << "loaded " << entries.size() << " rows and skipped " << malformed << " malformed rows from " << csvPath;
        error = ss.str();
    } else {
        std::ostringstream ss;
        ss << "loaded " << entries.size() << " rows from " << csvPath;
        error = ss.str();
    }

    return true;
}

std::string RegisterLogPlotter::makeOutputPath(const Options &options, const std::string &suffix) const {
    if (options.outputPrefix.empty()) {
        return options.outputDir + "/" + suffix;
    }
    return options.outputDir + "/" + options.outputPrefix + "-" + suffix;
}

bool RegisterLogPlotter::parseHex32(const std::string &value, uint32_t &result) {
    std::string s = trim(value);
    if (s.empty()) {
        return false;
    }

    std::size_t idx = 0;
    unsigned long parsed = 0;
    try {
        parsed = std::stoul(s, &idx, 16);
    } catch (...) {
        return false;
    }

    if (idx != s.size() || parsed > 0xffffffffUL) {
        return false;
    }

    result = static_cast<uint32_t>(parsed);
    return true;
}

bool RegisterLogPlotter::parseBoolFlag(const std::string &value, bool &result) {
    std::string s = toLower(trim(value));
    if (s == "1" || s == "true") {
        result = true;
        return true;
    }

    if (s == "0" || s == "false") {
        result = false;
        return true;
    }

    return false;
}

std::vector<std::string> RegisterLogPlotter::splitCsvLine(const std::string &line) {
    std::vector<std::string> cols;
    std::stringstream ss(line);
    std::string item;
    while (std::getline(ss, item, ',')) {
        cols.push_back(item);
    }
    return cols;
}

std::string RegisterLogPlotter::trim(const std::string &value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }

    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return value.substr(start, end - start);
}

std::string RegisterLogPlotter::toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string RegisterLogPlotter::categoryKey(const std::string &category) {
    std::string key = toLower(trim(category));
    if (key == "cr") {
        return "CR";
    }
    if (key == "sr") {
        return "SR";
    }
    if (key == "dr") {
        return "DR";
    }
    return "UNKNOWN";
}

void RegisterLogPlotter::plotCategoryCounts(const std::map<std::string, uint64_t> &counts, const Options &options,
                                            std::vector<std::string> &generatedPlots) const {
    using namespace matplot;

    const std::vector<std::string> labels = {"CR", "SR", "DR", "UNKNOWN"};
    std::vector<double> ticks;
    std::vector<double> values;
    ticks.reserve(labels.size());
    values.reserve(labels.size());
    for (std::size_t i = 0; i < labels.size(); ++i) {
        const auto &label = labels[i];
        ticks.push_back(static_cast<double>(i + 1));
        auto it = counts.find(label);
        values.push_back(it == counts.end() ? 0.0 : static_cast<double>(it->second));
    }

    auto f = figure(true);
    f->size(1200, 800);
    bar(values);
    title("Register count by category");
    xticks(ticks);
    xticklabels(labels);
    xlabel("Register category");
    ylabel("Count");
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (values[i] > 0) {
            text(i + 1, values[i] + values[i] * 0.02, std::to_string(static_cast<uint64_t>(values[i])));
        }
    }
    const std::string path = makeOutputPath(options, "registers-by-category.png");
    save(path, "png");
    generatedPlots.push_back(path);
}

void RegisterLogPlotter::plotTopBaseAddresses(const std::map<uint32_t, uint64_t> &baseCounts, const Options &options,
                                              std::vector<std::string> &generatedPlots) const {
    using namespace matplot;

    std::vector<std::pair<uint32_t, uint64_t>> items(baseCounts.begin(), baseCounts.end());
    std::sort(items.begin(), items.end(), [](const auto &a, const auto &b) {
        if (a.second != b.second) {
            return a.second > b.second;
        }
        return a.first < b.first;
    });

    if (items.empty()) {
        return;
    }

    std::size_t topN = std::min<std::size_t>(options.topNBaseAddresses, items.size());
    std::vector<double> ticks;
    std::vector<double> values;
    std::vector<std::string> labels;
    ticks.reserve(topN);
    values.reserve(topN);
    labels.reserve(topN);
    for (std::size_t i = 0; i < topN; ++i) {
        ticks.push_back(static_cast<double>(i + 1));
        values.push_back(static_cast<double>(items[i].second));

        std::ostringstream ss;
        ss << "0x" << std::hex << std::nouppercase << std::setw(8) << std::setfill('0') << items[i].first;
        labels.push_back(ss.str());
    }

    auto f = figure(true);
    f->size(1400, 900);
    bar(values);
    title("Top base addresses by touched registers");
    xticks(ticks);
    xticklabels(labels);
    xtickangle(45);
    xlabel("Base address");
    ylabel("Touched registers");
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (values[i] > 0) {
            text(i + 1, values[i] + values[i] * 0.02, std::to_string(static_cast<uint64_t>(values[i])));
        }
    }
    const std::string path = makeOutputPath(options, "top-base-addresses.png");
    save(path, "png");
    generatedPlots.push_back(path);
}

void RegisterLogPlotter::plotReadWriteMix(const std::map<std::string, std::pair<uint64_t, uint64_t>> &rwByCategory,
                                          const Options &options, std::vector<std::string> &generatedPlots) const {
    using namespace matplot;

    const std::vector<std::string> labels = {"CR", "SR", "DR", "UNKNOWN"};
    std::vector<double> ticks;
    std::vector<double> readCounts;
    std::vector<double> writeCounts;
    ticks.reserve(labels.size());
    readCounts.reserve(labels.size());
    writeCounts.reserve(labels.size());

    for (std::size_t i = 0; i < labels.size(); ++i) {
        const auto &label = labels[i];
        ticks.push_back(static_cast<double>(i + 1));
        auto it = rwByCategory.find(label);
        if (it == rwByCategory.end()) {
            readCounts.push_back(0.0);
            writeCounts.push_back(0.0);
            continue;
        }

        readCounts.push_back(static_cast<double>(it->second.first));
        writeCounts.push_back(static_cast<double>(it->second.second));
    }

    auto f = figure(true);
    f->size(1200, 800);
    hold(on);
    auto b1 = bar(readCounts);
    b1->display_name("Read");
    auto b2 = bar(writeCounts);
    b2->display_name("Write");
    hold(off);

    title("Read/write mix by category");
    xticks(ticks);
    xticklabels(labels);
    xlabel("Register category");
    ylabel("Count");
    legend();

    const std::string path = makeOutputPath(options, "rw-mix-by-category.png");
    save(path, "png");
    generatedPlots.push_back(path);
}

void RegisterLogPlotter::plotRegisterTimeline(const std::vector<CsvEntry> &entries, const Options &options,
                                              std::vector<std::string> &generatedPlots) const {
    using namespace matplot;

    if (entries.empty()) {
        return;
    }

    double minTime = std::numeric_limits<double>::max();
    double maxTime = std::numeric_limits<double>::lowest();
    for (const auto &e : entries) {
        minTime = std::min(minTime, e.timestamp);
        maxTime = std::max(maxTime, e.timestamp);
    }

    if (minTime == maxTime) {
        return;
    }

    constexpr std::size_t numBins = 15;
    constexpr std::size_t smoothWindow = 3;
    double binSize = (maxTime - minTime) / numBins;
    double binSizeUs = binSize * 1e6;
    if (binSize <= 0) {
        return;
    }

    std::vector<double> crCounts(numBins, 0.0);
    std::vector<double> srCounts(numBins, 0.0);
    std::vector<double> drCounts(numBins, 0.0);
    std::vector<double> binCenters(numBins);

    for (std::size_t i = 0; i < numBins; ++i) {
        binCenters[i] = (minTime + (i + 0.5) * binSize) * 1e6;
    }

    for (const auto &e : entries) {
        std::size_t binIdx = static_cast<std::size_t>((e.timestamp - minTime) / binSize);
        if (binIdx >= numBins) {
            binIdx = numBins - 1;
        }

        std::string key = categoryKey(e.category);
        if (key == "CR") {
            crCounts[binIdx]++;
        } else if (key == "SR") {
            srCounts[binIdx]++;
        } else if (key == "DR") {
            drCounts[binIdx]++;
        }
    }

    auto smoothData = [&](std::vector<double> &data) {
        std::vector<double> smoothed(data.size(), 0.0);
        for (std::size_t i = 0; i < data.size(); ++i) {
            double sum = 0.0;
            std::size_t count = 0;
            for (std::size_t j = (i >= smoothWindow ? i - smoothWindow + 1 : 0);
                 j <= std::min(i + smoothWindow - 1, data.size() - 1); ++j) {
                sum += data[j];
                count++;
            }
            smoothed[i] = sum / count;
        }
        return smoothed;
    };

    crCounts = smoothData(crCounts);
    srCounts = smoothData(srCounts);
    drCounts = smoothData(drCounts);

    auto f = figure(true);
    f->size(1400, 900);

    std::vector<double> xTicks;
    for (std::size_t i = 0; i < numBins; ++i) {
        xTicks.push_back(static_cast<double>(i + 1));
    }

    auto ah = gca();
    ah->hold(on);

    auto b1 = bar(drCounts);
    b1->display_name("DR");
    auto b2 = bar(srCounts);
    b2->display_name("SR");
    auto b3 = bar(crCounts);
    b3->display_name("CR");

    ah->hold(off);

    std::ostringstream ssLabel;
    ssLabel << std::fixed << std::setprecision(0) << minTime * 1e6;
    std::string startLabel = ssLabel.str();
    ssLabel.str("");
    ssLabel << std::fixed << std::setprecision(0) << maxTime * 1e6;
    std::string endLabel = ssLabel.str();

    title("Register access timeline (" + startLabel + "us - " + endLabel + "us)");
    xticks(xTicks);
    std::ostringstream ssBin;
    ssBin << std::fixed << std::setprecision(1) << binSizeUs;
    xlabel("Time bucket (avg bin: " + ssBin.str() + "us)");
    ylabel("Aggregated register access");
    legend();

    const std::string path = makeOutputPath(options, "register-timeline.png");
    save(path, "png");
    generatedPlots.push_back(path);
}

} // namespace hw
} // namespace plugins
} // namespace s2e
