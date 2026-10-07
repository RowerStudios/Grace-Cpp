#pragma once

// f3 debug overlay. built in rather than loaded from a scene file, so it stays
// available even when a scene is broken.

#include <cstddef>
#include <string>
#include "scene_loader.h"

namespace grace {
namespace gui {

// one frame's worth of engine numbers, filled in by the caller
struct DebugInfo {
    float fps = 0.0f;
    float frame_ms = 0.0f;
    std::string scene;
    std::string scene_source;
    std::string scene_entry;
    size_t scene_nodes = 0;
    int scene_mains = 0;
    int window_w = 0;
    int window_h = 0;
    int pixel_w = 0;
    int pixel_h = 0;
    int draw_calls = 0;
    int vertices = 0;
    std::string gl_version;
    std::string gl_renderer;
    std::string last_event;
};

// top left overlay box. needs a current gl context and a live renderer.
void draw_debug_panel(const DebugInfo& info);

}  // namespace gui
}  // namespace grace