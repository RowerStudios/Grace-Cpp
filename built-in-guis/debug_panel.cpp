#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>
#include "renderer.h"
#include "debug_panel.h"

namespace grace {
namespace gui {
namespace {

std::string format(const char* fmt, ...) {
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    return std::string(buffer);
}
struct Line {
    std::string text;
    Vec3 color;
};
}  // namespace
void draw_debug_panel(const DebugInfo& info) {
    constexpr float kInset = 12.0f;
    constexpr float kPadX = 12.0f;
    constexpr float kPadY = 10.0f;
    constexpr float kFontSize = 13.0f;
    constexpr float kLineHeight = 18.0f;
    Vec3 fps_color = info.fps >= 55.0f   ? Vec3{0.45f, 0.95f, 0.55f}
                     : info.fps >= 30.0f ? Vec3{1.0f, 0.85f, 0.35f}
                                        : Vec3{1.0f, 0.45f, 0.4f};
    std::vector<Line> lines;
    lines.push_back({format("fps %.1f   frame %.2f ms", info.fps, info.frame_ms), fps_color});
    lines.push_back({format("scene %s", info.scene.c_str()), {0.90f, 0.90f, 0.95f}});
    lines.push_back({format("  entry %s", info.scene_entry.c_str()), {0.65f, 0.68f, 0.75f}});
    lines.push_back({format("  %s", info.scene_source.c_str()), {0.55f, 0.58f, 0.65f}});
    lines.push_back({format("nodes %zu   main nodes %d", info.scene_nodes, info.scene_mains),
                     {0.80f, 0.82f, 0.88f}});
    lines.push_back({format("window %dx%d   drawable %dx%d", info.window_w, info.window_h,
                            info.pixel_w, info.pixel_h),
                     {0.80f, 0.82f, 0.88f}});
    lines.push_back({format("draws %d   verts %d", info.draw_calls, info.vertices),
                     {0.80f, 0.82f, 0.88f}});
    lines.push_back({format("gl %s", info.gl_version.c_str()), {0.70f, 0.75f, 0.85f}});
    lines.push_back({format("gpu %s", info.gl_renderer.c_str()), {0.70f, 0.75f, 0.85f}});
    if (!info.last_event.empty()) {
        lines.push_back({format("last event %s", info.last_event.c_str()), {0.60f, 0.62f, 0.70f}});
    }
    lines.push_back({"f3 toggles this panel", {0.50f, 0.52f, 0.60f}});

    float widest = 0.0f;
    for (const Line& line : lines) {
        widest = std::max(widest, measure_text(line.text, kFontSize));
    }

    float panel_w = widest + kPadX * 2.0f;
    float panel_h = static_cast<float>(lines.size()) * kLineHeight + kPadY * 2.0f;

    draw_rect(kInset, kInset, panel_w, panel_h, {0.05f, 0.06f, 0.08f, 0.82f});

    float y = kInset + kPadY + kLineHeight * 0.5f;
    for (const Line& line : lines) {
        draw_text_left(line.text, kInset + kPadX + 2.0f, y, kFontSize, line.color);
        y += kLineHeight;
    }
}

}  // namespace gui
}  // namespace grace
