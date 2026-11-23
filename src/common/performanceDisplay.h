#pragma once

#include "common/hostStats.h"
#include <cmath>

namespace Common::PerformanceDisplay {
enum class Severity { Neutral, Good, Warning, Bad };
inline bool Valid(std::optional<double> value) {
    return value && std::isfinite(*value) && *value >= 0.0;
}
inline Severity Classify(std::optional<double> value, double warn, double bad) {
    if (!Valid(value)) return Severity::Neutral;
    const double rounded = std::round(*value);
    return rounded < warn ? Severity::Good : rounded < bad ? Severity::Warning : Severity::Bad;
}
inline std::optional<CpuUsage> BusiestCpu(const std::vector<CpuUsage>& cpus) {
    std::optional<CpuUsage> best;
    for (const auto& cpu : cpus) {
        if (!Valid(cpu.percent) || *cpu.percent > 100.0) continue;
        if (!best || *cpu.percent > *best->percent ||
            (*cpu.percent == *best->percent && cpu.id < best->id)) best = cpu;
    }
    return best;
}
inline std::optional<double> RamPercent(std::optional<uint64_t> used, std::optional<uint64_t> total) {
    if (!used || !total || *total == 0 || *used > *total) return std::nullopt;
    return 100.0 * static_cast<double>(*used) / static_cast<double>(*total);
}
} // namespace Common::PerformanceDisplay
