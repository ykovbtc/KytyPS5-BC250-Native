#pragma once

#include <chrono>
#include <cstdint>

namespace Libs::VideoOut {

// Keep the fractional phase between vblanks so a 55 FPS cap on a 60 Hz
// display can alternate one- and two-vblank intervals.
class GameFramePacer {
public:
	using Clock = std::chrono::steady_clock;

	explicit GameFramePacer(uint32_t fps)
	    : m_period(fps == 0 ? Clock::duration::zero() : Clock::duration(std::chrono::seconds(1)) / fps) {}

	bool Enabled() const { return m_period != Clock::duration::zero(); }
	bool Due(Clock::time_point now) const { return now >= m_next; }
	void Reset() { m_next = Clock::time_point::min(); }

	void Advance(Clock::time_point now) {
		// A late frame gets one slot. A long stall earns no catch-up burst.
		if (m_next <= now - m_period) {
			m_next = now;
		}
		m_next += m_period;
	}

private:
	Clock::duration   m_period;
	Clock::time_point m_next = Clock::time_point::min();
};

} // namespace Libs::VideoOut
