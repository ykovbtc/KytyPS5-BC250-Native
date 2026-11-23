#pragma once

#include <algorithm>
#include <cstdint>
#include <utility>

namespace NativeUpscaler {

// HDMI modes and fixed HLX output choices are 16:9. Zero keeps the existing limit.
constexpr int HlxHeightLimit(int hdmi, int requested) {
	return std::min(hdmi ? hdmi : 2160, requested ? requested : 2160);
}

constexpr std::pair<uint32_t, uint32_t> LimitHlxExtent(uint32_t width, uint32_t height, int limit) {
	const auto h = static_cast<uint32_t>(limit);
	return {std::min(width, h * 16 / 9), std::min(height, h)};
}
// The helper dispatch is bounded by both the active guest rectangle and its allocated output texture.
constexpr std::pair<uint32_t, uint32_t> HlxDispatchExtent(uint32_t guest_w, uint32_t guest_h, uint32_t texture_w, uint32_t texture_h) {
	return {std::min(guest_w, texture_w), std::min(guest_h, texture_h)};
}
} // namespace NativeUpscaler
