#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>

namespace Common::HostStatsLinux {

struct CpuTicks { uint64_t busy = 0, total = 0; };

inline std::optional<CpuTicks> ParseCpuTicks(std::istream& input) {
	CpuTicks ticks;
	uint64_t value = 0, idle = 0;
	for (int i = 0; i < 8; ++i) {
		if (!(input >> value)) return std::nullopt;
		ticks.total += value;
		if (i == 3 || i == 4) idle += value;
	}
	ticks.busy = ticks.total - idle;
	return ticks;
}

inline std::optional<double> CpuPercent(CpuTicks now, CpuTicks last) {
	if (now.total <= last.total || now.busy < last.busy ||
	    now.total - now.busy < last.total - last.busy) return std::nullopt;
	return std::clamp(100.0 * static_cast<double>(now.busy - last.busy) /
	                  static_cast<double>(now.total - last.total), 0.0, 100.0);
}

inline std::optional<uint64_t> ParseUnsigned(const std::string& text) {
	if (text.empty() || text.front() == '-') return std::nullopt;
	std::istringstream stream(text);
	stream >> std::ws;
	if (stream.peek() == '-') return std::nullopt;
	uint64_t value;
	if (!(stream >> value)) return std::nullopt;
	stream >> std::ws;
	if (!stream.eof()) return std::nullopt;
	return value;
}

inline std::optional<uint64_t> ClockMHz(const std::string& hz) {
	const auto value = ParseUnsigned(hz);
	if (!value || *value < 1000000) return std::nullopt;
	return *value / 1000000;
}

inline std::optional<uint64_t> ActiveClockMHz(std::istream& levels) {
	std::string line;
	while (std::getline(levels, line)) {
		std::istringstream row(line);
		unsigned level;
		char colon, active;
		std::string unit;
		uint64_t mhz;
		if (!(row >> level >> colon) || colon != ':') continue;
		row >> std::ws;
		if (row.peek() == '-' || !(row >> mhz >> unit >> active) || !mhz || active != '*') continue;
		if (unit == "Mhz" || unit == "MHz" || unit == "mhz") return mhz;
	}
	return std::nullopt;
}

inline std::optional<double> Temperature(const std::string& text) {
	const auto value = ParseUnsigned(text);
	if (!value || *value > 150000) return std::nullopt;
	return static_cast<double>(*value) / 1000.0;
}

} // namespace Common::HostStatsLinux
