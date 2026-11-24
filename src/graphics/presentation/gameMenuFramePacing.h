#pragma once

#include <algorithm>
#include <cstdint>

namespace Libs::Graphics::GameMenuFramePacing {

enum MainItem : int { ItemResume, ItemMonitoring, ItemUpscaling, ItemFramePacing, ItemCheats, ItemExit, MainItemCount };

constexpr bool Available(uint32_t fps) { return fps >= 20 && fps <= 120; }

constexpr uint32_t Step(uint32_t fps, int direction) {
	return Available(fps) ? static_cast<uint32_t>(std::clamp(static_cast<int>(fps) + direction, 20, 120)) : fps;
}

constexpr int NextItem(int selected, int direction, uint32_t fps) {
	int next = (selected + direction + MainItemCount) % MainItemCount;
	return next == ItemFramePacing && !Available(fps) ? next + direction : next;
}

} // namespace Libs::Graphics::GameMenuFramePacing
