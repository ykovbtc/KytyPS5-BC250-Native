#pragma once

#include "common/hostStatsLinux.h"

#include <chrono>
#include <filesystem>
#include <fstream>

namespace Common::HostStatsLinux {

class Bc250VramTemperature {
public:
	using Clock = std::chrono::steady_clock;

	// A nonzero session identifies one period of continuous Performance monitoring.
	std::optional<double> Sample(const std::filesystem::path& hwmon, Clock::time_point now,
	                             uint64_t session) {
		if (session != m_session) {
			m_session = session;
			m_value.reset();
		}
		if (session == 0 || (m_next_read && now < *m_next_read)) {
			return m_value;
		}

		m_next_read = now + std::chrono::seconds(5);
		// hwmon indices can be reused when the driver is unloaded and loaded again.
		if (m_sensor.empty() || !IsSensor(m_sensor)) {
			m_sensor = FindSensor(hwmon);
		}
		m_value = m_sensor.empty() ? std::nullopt : Temperature(ReadLine(m_sensor / "temp1_input"));
		if (!m_value) {
			m_sensor.clear();
		}
		return m_value;
	}

private:
	static std::string ReadLine(const std::filesystem::path& path) {
		std::ifstream file(path);
		std::string value;
		std::getline(file, value);
		return value;
	}

	static bool IsSensor(const std::filesystem::path& path) {
		return ReadLine(path / "name") == "bc250_memory" &&
		       ReadLine(path / "temp1_label") == "VRAM Hotspot";
	}

	static std::filesystem::path FindSensor(const std::filesystem::path& hwmon) {
		std::error_code error;
		for (std::filesystem::directory_iterator it(hwmon, error), end; !error && it != end;
		     it.increment(error)) {
			const auto& path = it->path();
			if (path.filename().string().starts_with("hwmon") && IsSensor(path)) {
				return path;
			}
		}
		return {};
	}

	std::filesystem::path m_sensor;
	std::optional<double> m_value;
	std::optional<Clock::time_point> m_next_read;
	uint64_t m_session = 0;
};

} // namespace Common::HostStatsLinux
