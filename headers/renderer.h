#pragma once

// text, shapes and the debug overlay. all of it needs a current gl context.

#include <cstddef>
#include <string>
#include <vector>
#include "scene_loader.h"

namespace grace {

// loads a ttf, bakes a glyph atlas, compiles the text and shape shaders.
// needs a gl context, so call it after SDL_GL_CreateContext.
bool init_renderer(const std::string& font_path = "");

void shutdown_renderer();

// draws text centred on x,y in pixels, y grows downwards
void draw_text(const std::string& text, float x, float y, float size,
               Vec3 color = {1.0f, 1.0f, 1.0f});

// draws text with its left edge at x, vertically centred on y
void draw_text_left(const std::string& text, float x, float y, float size,
                    Vec3 color = {1.0f, 1.0f, 1.0f});

// width in pixels that draw_text would use for this text at this size
float measure_text(const std::string& text, float size);

// filled axis aligned rect, x/y is the top left corner
void draw_rect(float x, float y, float w, float h, Vec4 color);

// the colour the scene is cleared to before anything is drawn. an empty scene
// shows nothing but this, so it defaults to black.
void set_background(Vec4 color);
Vec4 background();

// draws every drawable node under the scene's main node, back to front
void render_scene(const Scene& scene, int viewport_width, int viewport_height);

struct FrameStats {
    int draw_calls = 0;
    int vertices = 0;
};

// counts for the frame render_scene drew last
FrameStats last_frame_stats();

}  // namespace grace