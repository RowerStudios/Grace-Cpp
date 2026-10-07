#pragma once

// engine settings, persisted through qsettings. lives outside the gui so the
// window, the menu and the game widget all read the same values.

#include <QSettings>

namespace grace {

enum class WindowMode {
    Windowed = 0,
    Fullscreen = 1,
    Borderless = 2,
};

enum class WindowScale {
    Resizable = 0,  // normal resizable window
    Custom = 1,     // exact size from custom_width/custom_height, not resizable
    Scaled = 2,     // fills the screen, renders at a fixed size, not resizable
};

struct AppSettings {
    bool vsync = true;
    int max_fps = 60;
    WindowMode window_mode = WindowMode::Windowed;
    WindowScale window_scale = WindowScale::Resizable;
    int custom_width = 1280;
    int custom_height = 720;

    // the size everything is authored against
    int render_width() const;
    int render_height() const;

    void load();
    void save() const;
};

// lets the menu tell "the user changed something" from "nothing to apply"
inline bool operator==(const AppSettings& a, const AppSettings& b) {
    return a.vsync == b.vsync && a.max_fps == b.max_fps && a.window_mode == b.window_mode &&
           a.window_scale == b.window_scale && a.custom_width == b.custom_width &&
           a.custom_height == b.custom_height;
}

inline bool operator!=(const AppSettings& a, const AppSettings& b) { return !(a == b); }

AppSettings& app_settings();

}  // namespace grace