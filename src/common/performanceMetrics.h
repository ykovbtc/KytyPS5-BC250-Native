#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <utility>
#include <functional>

namespace Common::Performance {

using Clock = std::chrono::steady_clock;

inline constexpr unsigned CpuColumns = 4;
inline constexpr unsigned CpuRows(unsigned count) { return (count + CpuColumns - 1) / CpuColumns; }
inline constexpr bool ValidMode(int mode) { return mode >= 0 && mode <= 3; }
inline constexpr int StepMode(int mode, int step) { return ((mode + step) % 4 + 4) % 4; }
inline constexpr int DetailsMode(int mode) { return mode == 2 ? 1 : 2; }

inline std::atomic<uint64_t> session_generation {0};

// Shared by the native worker and presenter. Dimensions describe observed active images.
struct Rendering {
	uint64_t session = session_generation.load(std::memory_order_acquire);
	std::string upscaler = "Unknown";
	std::string preset = "Unknown";
	uint32_t render_width = 0, render_height = 0;
	uint32_t output_width = 0, output_height = 0;
	Clock::time_point observed {};
};

struct Sample {
	std::optional<double> low_fps;
	double presented_fps = 0;
	bool generated = false;
	Rendering rendering;
	uint64_t session_seconds = 0;
};

inline std::string SessionTime(uint64_t seconds) {
	return std::to_string(seconds / 3600) + "h " + std::to_string(seconds / 60 % 60) +
	       "m " + std::to_string(seconds % 60) + "s";
}

class Metrics {
public:
	void StartSession(Clock::time_point now) {
		m_session = now;
		m_rendering = {};
		ResetFrames(now);
	}
	void ResetFrames(Clock::time_point now) {
		m_intervals.clear();
		m_last_flip = {};
		m_window = now;
		m_presents = m_generated = 0;
	}
	void Flip(Clock::time_point now) {
		if (m_last_flip != Clock::time_point {} && now > m_last_flip) {
			m_intervals.push_back({now, std::chrono::duration<double, std::milli>(now - m_last_flip).count()});
		}
		m_last_flip = now;
		Prune(now);
	}
	void Present(bool generated) {
		++m_presents;
		if (generated) ++m_generated;
	}
	void Observe(Rendering rendering) { m_rendering = std::move(rendering); }
	Sample Read(Clock::time_point now) {
		Prune(now);
		Sample result;
		if (m_session != Clock::time_point {} && now >= m_session) {
			result.session_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - m_session).count();
		}
		const double elapsed = std::chrono::duration<double>(now - m_window).count();
		result.presented_fps = elapsed > 0 ? m_presents / elapsed : 0;
		result.generated = m_generated != 0;
		m_presents = m_generated = 0;
		m_window = now;
		if (m_rendering.observed != Clock::time_point {} && now - m_rendering.observed < std::chrono::seconds(2)) {
			result.rendering = m_rendering;
		}
		if (m_intervals.size() >= 100) {
			std::vector<double> intervals;
			for (const auto& interval: m_intervals) intervals.push_back(interval.ms);
			std::sort(intervals.begin(), intervals.end(), std::greater<double>());
			const size_t count = (intervals.size() + 99) / 100;
			double sum = 0;
			for (size_t i = 0; i < count; ++i) sum += intervals[i];
			if (sum > 0) result.low_fps = 1000.0 * count / sum;
		}
		return result;
	}
private:
	struct Interval { Clock::time_point time; double ms; };
	void Prune(Clock::time_point now) {
		while (!m_intervals.empty() && now - m_intervals.front().time >= std::chrono::seconds(30)) {
			m_intervals.pop_front();
		}
	}
	Clock::time_point m_session {}, m_last_flip {}, m_window {};
	std::deque<Interval> m_intervals;
	uint64_t m_presents = 0, m_generated = 0;
	Rendering m_rendering;
};

inline std::mutex mutex;
inline Metrics metrics;

inline void StartSession() {
	std::lock_guard lock(mutex);
	session_generation.fetch_add(1, std::memory_order_acq_rel);
	metrics.StartSession(Clock::now());
}
inline void Observe(Rendering rendering) {
	rendering.observed = Clock::now();
	std::lock_guard lock(mutex);
	if (rendering.session == session_generation.load(std::memory_order_acquire)) {
		metrics.Observe(std::move(rendering));
	}
}

} // namespace Common::Performance
