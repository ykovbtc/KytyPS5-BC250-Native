#include "graphics/presentation/gameFramePacer.h"

#include <cstdio>

using Libs::VideoOut::GameFramePacer;
using namespace std::chrono;

static int CountFrames(unsigned cap, unsigned refresh, unsigned guest_interval = 1) {
	GameFramePacer pacer(cap);
	int frames = 0;
	for (unsigned tick = 0; tick < refresh * 10; tick += guest_interval) {
		auto now = GameFramePacer::Clock::time_point(nanoseconds(1'000'000'000ULL * tick / refresh));
		if (!pacer.Enabled() || pacer.Due(now)) {
			if (pacer.Enabled()) {
				pacer.Advance(now);
			}
			++frames;
		}
	}
	return frames;
}

int main() {
	for (auto cap: {48u, 55u}) {
		for (auto refresh: {60u, 120u}) {
			if (CountFrames(cap, refresh) != static_cast<int>(cap * 10)) {
				std::fprintf(stderr, "Unexpected cadence for %u FPS at %u Hz\n", cap, refresh);
				return 1;
			}
		}
	}
	if (CountFrames(0, 60) != 600 || CountFrames(120, 60) != 600 || CountFrames(48, 60, 2) != 300 ||
	    CountFrames(1, 60) != 10) {
		return 1;
	}
	GameFramePacer pacer(50);
	GameFramePacer::Clock::time_point now {};
	pacer.Advance(now);
	now += seconds(5);
	if (!pacer.Due(now)) {
		return 1;
	}
	pacer.Advance(now);
	if (pacer.Due(now + milliseconds(19)) || !pacer.Due(now + milliseconds(20))) {
		return 1;
	}
	pacer.Reset();
	return pacer.Due(now) ? 0 : 1;
}
