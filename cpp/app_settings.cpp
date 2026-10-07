#include "app_settings.h"

#include <algorithm>

namespace grace {
namespace {

// what the Scaled mode renders at, then stretches to the screen
constexpr int kScaledWidth = 1920;
constexpr int kScaledHeight = 1080;

}  // namespace

int AppSettings::render_width() const {
    if (window_scale == WindowScale::Scaled) return kScaledWidth;
    return custom_width;
}

int AppSettings::render_height() const {
    if (window_scale == WindowScale::Scaled) return kScaledHeight;
    return custom_height;
}

void AppSettings::load() {
    QSettings store("RowerStudios", "GraceCpp");
    vsync = store.value("display/vsync", vsync).toBool();
    max_fps = std::clamp(store.value("display/max_fps", max_fps).toInt(), 1, 1000);
    window_mode = static_cast<WindowMode>(
        std::clamp(store.value("display/window_mode", static_cast<int>(window_mode)).toInt(), 0, 2));
    window_scale = static_cast<WindowScale>(
        std::clamp(store.value("display/window_scale", static_cast<int>(window_scale)).toInt(), 0, 2));
    custom_width =
        std::clamp(store.value("display/custom_width", custom_width).toInt(), 320, 7680);
    custom_height =
        std::clamp(store.value("display/custom_height", custom_height).toInt(), 240, 4320);
}

void AppSettings::save() const {
    QSettings store("RowerStudios", "GraceCpp");
    store.setValue("display/vsync", vsync);
    store.setValue("display/max_fps", max_fps);
    store.setValue("display/window_mode", static_cast<int>(window_mode));
    store.setValue("display/window_scale", static_cast<int>(window_scale));
    store.setValue("display/custom_width", custom_width);
    store.setValue("display/custom_height", custom_height);
}

AppSettings& app_settings() {
    static AppSettings instance;
    static bool loaded = false;
    if (!loaded) {
        instance.load();
        loaded = true;
    }
    return instance;
}

}  // namespace grace