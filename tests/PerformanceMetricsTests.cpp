#include "common/bc250VramTemperature.h"
#include "common/performanceMetrics.h"
#include "common/hostStatsLinux.h"
#include "common/performanceDisplay.h"
#include <limits>
#include "graphics/native_gpu/upscaler/hlxOutputSize.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace Common::Performance;
using namespace std::chrono_literals;

static void Check(bool ok, const char* message) {
	if (!ok) {
		std::fprintf(stderr, "FAIL: %s\n", message);
		std::exit(1);
	}
}


static void CheckBc250VramTemperature() {
	using Common::HostStatsLinux::Bc250VramTemperature;
	namespace fs = std::filesystem;
	const auto now = Bc250VramTemperature::Clock::now();
	const auto hwmon = fs::temp_directory_path() /
	                   ("kyty-bc250-hwmon-" + std::to_string(now.time_since_epoch().count()));
	Check(fs::create_directory(hwmon), "create private hwmon fixture");
	const auto sensor = hwmon / "hwmon7";
	fs::create_directory(sensor);
	const auto write = [&](const char* file, const char* value) {
		Check(bool(std::ofstream(sensor / file) << value << '\n'), "write hwmon fixture");
	};
	Bc250VramTemperature vram;
	write("name", "amdgpu");
	write("temp1_label", "VRAM Hotspot");
	write("temp1_input", "79000");
	Check(!vram.Sample(hwmon, now, 1), "ignore a different hwmon driver");
	write("name", "bc250_memory");
	write("temp1_label", "VRAM Average");
	Check(!vram.Sample(hwmon, now + 5s, 1), "require the VRAM Hotspot channel");
	write("temp1_label", "VRAM Hotspot");
	Check(!vram.Sample(hwmon, now + 6s, 1), "rate limit missing sensor discovery");
	Check(!vram.Sample(hwmon, now + 10s, 0), "inactive monitoring does not sample");
	Check(vram.Sample(hwmon, now + 10s, 2) == 79.0, "read the hottest GDDR6 chip");

	write("temp1_input", "broken");
	Check(vram.Sample(hwmon, now + 14s, 2) == 79.0, "reuse the prior reading before five seconds");
	Check(!vram.Sample(hwmon, now + 15s, 2), "invalid read clears the prior temperature");
	fs::remove(sensor / "temp1_input");
	Check(!vram.Sample(hwmon, now + 20s, 2), "missing input stays unavailable");
	write("temp1_input", "80500");
	Check(vram.Sample(hwmon, now + 25s, 2) == 80.5, "re-discover a working sensor");
	Check(!vram.Sample(hwmon, now + 26s, 0), "leaving Performance clears the cached reading");
	Check(!vram.Sample(hwmon, now + 27s, 3), "re-entering Performance preserves the rate limit");
	Check(vram.Sample(hwmon, now + 30s, 3) == 80.5, "resume polling after the rate limit");
	Check(!vram.Sample(hwmon, now + 31s, 4), "session changes clear cache even between samples");

	const auto moved = hwmon / "hwmon12";
	fs::rename(sensor, moved);
	fs::create_directory(sensor);
	write("name", "amdgpu");
	write("temp1_input", "45000");
	Check(vram.Sample(hwmon, now + 35s, 4) == 80.5, "find the driver after a hwmon index change");
	fs::remove_all(hwmon);
	Check(!vram.Sample(hwmon, now + 40s, 4), "missing hwmon tree is handled without throwing");
}

int main() {
	Check(StepMode(0, 1) == 1 && StepMode(2, 1) == 3 && StepMode(3, 1) == 0 && StepMode(0, -1) == 3,
	      "four modes wrap both directions");
	Check(!ValidMode(-1) && !ValidMode(4) && ValidMode(3), "invalid modes rejected");
	Check(DetailsMode(0) == 2 && DetailsMode(1) == 2 && DetailsMode(2) == 1 && DetailsMode(3) == 2,
	      "details shortcut from all modes");
	Check(CpuColumns == 4 && CpuRows(1) == 1 && CpuRows(4) == 1 && CpuRows(5) == 2 &&
	      CpuRows(8) == 2 && CpuRows(12) == 3, "four logical CPUs per row");
	Check(SessionTime(0) == "0h 0m 0s" && SessionTime(59) == "0h 0m 59s" &&
	      SessionTime(60) == "0h 1m 0s" && SessionTime(3600) == "1h 0m 0s" &&
	      SessionTime(5124) == "1h 25m 24s" && SessionTime(360000) == "100h 0m 0s", "session formatting");

	Metrics metrics;
	const auto start = Clock::time_point(1s);
	metrics.StartSession(start);
	metrics.Flip(start);
	auto now = start;
	for (unsigned i = 0; i < 99; ++i) { now += 10ms; metrics.Flip(now); }
	Check(!metrics.Read(now).low_fps, "1% low needs 100 intervals");
	now += 100ms;
	metrics.Flip(now);
	Check(std::abs(*metrics.Read(now).low_fps - 10.0) < 0.001, "slowest interval of 100");
	for (unsigned i = 0; i < 99; ++i) { now += 10ms; metrics.Flip(now); }
	now += 50ms;
	metrics.Flip(now);
	Check(std::abs(*metrics.Read(now).low_fps - 1000.0 / 75.0) < 0.001, "average slowest two of 200");
	Check(!metrics.Read(now + 31s).low_fps, "old intervals expire during stalls");
	Check(metrics.Read(now + 32s).presented_fps == 0, "stalled presentations are zero");

	metrics.ResetFrames(start + 60s);
	for (int i = 0; i < 60; ++i) { metrics.Present(false); metrics.Present(true); }
	auto sample = metrics.Read(start + 61s);
	Check(sample.presented_fps == 120 && sample.generated, "real and generated presentations counted");
	Check(sample.session_seconds == 61, "hiding and enabling monitoring preserves session start");
	Check(!metrics.Read(start + 62s).generated, "inactive generation clears next interval");
	Rendering rendering;
	rendering.upscaler = "HLX FSR 4";
	rendering.observed = start + 62s;
	metrics.Observe(rendering);
	Check(metrics.Read(start + 63s).rendering.upscaler == "HLX FSR 4", "fresh observation");
	Check(metrics.Read(start + 65s).rendering.upscaler == "Unknown", "stale observation expires");
	rendering.upscaler = "Spatial fallback";
	rendering.observed = start + 65s;
	metrics.Observe(rendering);
	Check(metrics.Read(start + 66s).rendering.upscaler == "Spatial fallback", "fallback replaces successful upscale");
	metrics.StartSession(start + 100s);
	sample = metrics.Read(start + 101s);
	Check(sample.session_seconds == 1 && sample.rendering.upscaler == "Unknown" && !sample.low_fps,
	      "new session clears observations and history");

	StartSession();
	Rendering previous_session;
	previous_session.upscaler = "HLX FSR 4";
	StartSession();
	Observe(previous_session);
	{
		std::lock_guard lock(Common::Performance::mutex);
		Check(Common::Performance::metrics.Read(Clock::now()).rendering.upscaler == "Unknown",
		      "late worker completion cannot restore previous session state");
	}

	using namespace Common::HostStatsLinux;
	std::istringstream cpu("10 2 8 70 10 0 0 0 999 999");
	const auto ticks = ParseCpuTicks(cpu);
	Check(ticks && ticks->busy == 20 && ticks->total == 100, "exclude guest duplication and count iowait as idle");
	Check(CpuPercent({40, 200}, {20, 100}) == 20.0, "CPU delta percentage");
	Check(!CpuPercent({20, 100}, {20, 100}) && !CpuPercent({5, 100}, {20, 200}), "zero delta and reset");
	std::istringstream bad("1 2 broken");
	Check(!ParseCpuTicks(bad), "malformed CPU counter");
	Check(ParseUnsigned("0") == 0 && ParseUnsigned("123\n") == 123 && !ParseUnsigned("-1") && !ParseUnsigned(" -1") &&
	      !ParseUnsigned("12watts") && !ParseUnsigned(""), "strict unsigned sensors");
	Check(Temperature("72000") == 72.0 && !Temperature("999999") && !Temperature("bad"), "temperature validation");

    using namespace Common::PerformanceDisplay;
    for (const auto thresholds : {std::pair{75., 81.}, {75., 85.}, {60., 75.},
                                  {94., 98.}, {70., 90.}, {70., 96.}}) {
        const auto [warn, bad] = thresholds;
        Check(Classify(warn - 1, warn, bad) == Severity::Good, "below warning");
        Check(Classify(warn, warn, bad) == Severity::Warning, "at warning");
        Check(Classify(warn + 1, warn, bad) == Severity::Warning, "above warning");
        Check(Classify(bad - 1, warn, bad) == Severity::Warning, "below red");
        Check(Classify(bad, warn, bad) == Severity::Bad, "at red");
        Check(Classify(bad + 1, warn, bad) == Severity::Bad, "above red");
        Check(Classify(warn - .51, warn, bad) == Severity::Good &&
              Classify(warn - .5, warn, bad) == Severity::Warning, "rounded warning");
        Check(Classify(bad - .51, warn, bad) == Severity::Warning &&
              Classify(bad - .5, warn, bad) == Severity::Bad, "rounded red");
    }
    Check(Classify(std::nullopt, 70, 90) == Severity::Neutral &&
          Classify(std::numeric_limits<double>::quiet_NaN(), 70, 90) == Severity::Neutral &&
          Classify(-1., 70, 90) == Severity::Neutral, "unavailable colors");
    Check(RamPercent(12, 16) == 75. && RamPercent(0, 16) == 0. &&
          !RamPercent(1, 0) && !RamPercent(std::nullopt, 16) &&
          !RamPercent(1, std::nullopt) && !RamPercent(17, 16), "RAM validation");
    std::vector<Common::CpuUsage> cpus{{9, 98.}, {3, 98.}, {0, 60.}, {12, std::nullopt}};
    Check(BusiestCpu(cpus)->id == 3, "ties choose lowest noncontiguous CPU ID");
    cpus.erase(cpus.begin() + 1);
    Check(BusiestCpu(cpus)->id == 9, "removed CPU cannot remain busiest");
    cpus.push_back({22, 99.});
    Check(BusiestCpu(cpus)->id == 22, "new CPU can become busiest");
    Check(!BusiestCpu({}) && !BusiestCpu({{0, std::nullopt}, {1, -1.}, {2, 101.}}),
          "no valid CPU samples");
	CheckBc250VramTemperature();
	Check(ClockMHz("2000000000") == 2000 && !ClockMHz("0") && !ClockMHz("bad"), "AMDGPU graphics clock in Hz");
	std::istringstream clocks("0: 500Mhz\n1: 2000Mhz *\n2: 2200Mhz\n");
	std::istringstream inactive("0: 500Mhz\n1: 2200Mhz\n");
	Check(ActiveClockMHz(clocks) == 2000 && !ActiveClockMHz(inactive), "only the active graphics DPM level");
	using NativeUpscaler::HlxHeightLimit;
	using NativeUpscaler::LimitHlxExtent;
	Check(HlxHeightLimit(1440, 1800) == 1440 && HlxHeightLimit(2160, 1080) == 1080 &&
	      HlxHeightLimit(0, 1800) == 1800 && HlxHeightLimit(0, 0) == 2160, "HDMI and HLX target precedence");
	Check(LimitHlxExtent(3840, 2160, 1440) == std::pair{2560u, 1440u} &&
	      LimitHlxExtent(3840, 2160, 1080) == std::pair{1920u, 1080u} &&
	      LimitHlxExtent(1920, 1080, 2160) == std::pair{1920u, 1080u}, "bound reconstruction without enlarging the title output");
	Check(NativeUpscaler::HlxDispatchExtent(3840, 2160, 2560, 1440) == std::pair{2560u, 1440u} &&
	      NativeUpscaler::HlxDispatchExtent(1920, 1080, 2560, 1440) == std::pair{1920u, 1080u}, "helper wire dimensions track the active rectangle");
	std::puts("Performance metrics tests passed");
}
