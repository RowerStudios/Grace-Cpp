#include <glad/gl.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <utility>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include "renderer.h"

namespace grace {
namespace {

constexpr int kFirstGlyph = 32;
constexpr int kLastGlyph = 126;
constexpr int kAtlasSize = 1024;
// baked big so smaller sizes downscale cleanly instead of looking blocky
constexpr float kAtlasFontPixels = 48.0f;
constexpr int kMaxGlyphsPerFrame = 8192;

struct Glyph {
    float u0 = 0, v0 = 0, u1 = 0, v1 = 0;
    float w = 0, h = 0;
    float xoff = 0;     // bitmap top left, relative to the glyph origin
    float yoff = 0;
    float advance = 0;
};

stbtt_fontinfo g_font;
unsigned char* g_font_data = nullptr;
std::vector<unsigned char> g_atlas;
Glyph g_glyphs[128];
float g_ascent = 0.0f;
float g_descent = 0.0f;

GLuint g_program = 0;
GLuint g_shape_program = 0;
GLuint g_vao = 0;
GLuint g_vbo = 0;
GLuint g_texture = 0;
GLint g_u_proj = -1;
GLint g_u_atlas = -1;
GLint g_u_color = -1;
GLint g_shape_u_proj = -1;
GLint g_shape_u_color = -1;
std::vector<float> g_vertices;
FrameStats g_frame_stats;
float g_viewport[2] = {1.0f, 1.0f};
Vec4 g_background{0.0f, 0.0f, 0.0f, 1.0f};
bool g_ready = false;

const char* kVertexSource = R"(#version 330 core
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
uniform mat4 u_proj;
out vec2 v_uv;
void main() {
    v_uv = a_uv;
    gl_Position = u_proj * vec4(a_pos, 0.0, 1.0);
}
)";

const char* kFragmentSource = R"(#version 330 core
in vec2 v_uv;
uniform sampler2D u_atlas;
uniform vec3 u_color;
out vec4 frag_color;
void main() {
    frag_color = vec4(u_color, texture(u_atlas, v_uv).r);
}
)";

// same vertex layout as the text shader, uv is ignored here
const char* kShapeVertexSource = R"(#version 330 core
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
uniform mat4 u_proj;
out vec2 v_uv;
void main() {
    v_uv = a_uv;
    gl_Position = u_proj * vec4(a_pos, 0.0, 1.0);
}
)";

const char* kShapeFragmentSource = R"(#version 330 core
in vec2 v_uv;
uniform vec4 u_color;
out vec4 frag_color;
void main() {
    frag_color = u_color;
}
)";

GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048] = {};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::fprintf(stderr, "shader compile failed: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint build_program(const char* vertex_source, const char* fragment_source) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_source);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (!vs || !fs) return 0;

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048] = {};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::fprintf(stderr, "program link failed: %s\n", log);
        return 0;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

// column major ortho where 0,0 is the top left pixel and w,h the bottom right
void ortho_top_down(float w, float h, float* m) {
    std::memset(m, 0, 16 * sizeof(float));
    m[0] = 2.0f / w;
    m[5] = -2.0f / h;
    m[10] = -1.0f;
    m[12] = -1.0f;
    m[13] = 1.0f;
    m[15] = 1.0f;
}

bool bake_atlas() {
    g_atlas.assign(kAtlasSize * kAtlasSize, 0);

    int ascent = 0, descent = 0, line_gap = 0;
    stbtt_GetFontVMetrics(&g_font, &ascent, &descent, &line_gap);
    float scale = stbtt_ScaleForPixelHeight(&g_font, kAtlasFontPixels);
    g_ascent = ascent * scale;
    // descent comes back negative from stb, keep the sign out of the maths
    g_descent = -descent * scale;

    int pen_x = 1;
    int pen_y = 1;

    for (int c = kFirstGlyph; c <= kLastGlyph; c++) {
        // fills corners, not width/height: w = ix1 - ix0, h = iy1 - iy0,
        // and the bitmap goes at (ix0, iy0) relative to the glyph origin
        int ix0 = 0, iy0 = 0, ix1 = 0, iy1 = 0;
        stbtt_GetCodepointBitmapBox(&g_font, c, scale, scale, &ix0, &iy0, &ix1, &iy1);
        int w = ix1 - ix0;
        int h = iy1 - iy0;
        int xoff = ix0;
        int yoff = iy0;

        int advance = 0, lsb = 0;
        stbtt_GetCodepointHMetrics(&g_font, c, &advance, &lsb);

        Glyph& g = g_glyphs[c];
        // the box is already in pixels, only the advance comes back in font units
        g.w = static_cast<float>(w);
        g.h = static_cast<float>(h);
        g.xoff = static_cast<float>(xoff);
        g.yoff = static_cast<float>(yoff);
        g.advance = advance * scale;

        if (w > 0 && h > 0) {
            unsigned char* bitmap = stbtt_GetCodepointBitmap(&g_font, 0, scale, c, &w, &h,
                                                            &xoff, &yoff);
            if (bitmap) {
                for (int row = 0; row < h; row++) {
                    std::memcpy(&g_atlas[(pen_y + row) * kAtlasSize + pen_x],
                                &bitmap[row * w], static_cast<size_t>(w));
                }
                stbtt_FreeBitmap(bitmap, nullptr);

                g.u0 = static_cast<float>(pen_x) / kAtlasSize;
                g.v0 = static_cast<float>(pen_y) / kAtlasSize;
                g.u1 = static_cast<float>(pen_x + w) / kAtlasSize;
                g.v1 = static_cast<float>(pen_y + h) / kAtlasSize;
            }
            pen_x += w + 1;
        }
        if (pen_x + 64 >= kAtlasSize) {
            pen_x = 1;
            pen_y += static_cast<int>(kAtlasFontPixels) + 8;
        }
    }
    return true;
}

void push_quad(float left, float top, float right, float bottom,
               float u0, float v0, float u1, float v1) {
    const float quad[24] = {
        left,  top,    u0, v0,
        right, top,    u1, v0,
        left,  bottom, u0, v1,

        left,  bottom, u0, v1,
        right, top,    u1, v0,
        right, bottom, u1, v1,
    };
    g_vertices.insert(g_vertices.end(), std::begin(quad), std::end(quad));
}

// uploads whatever is in g_vertices with the given program and draws it
void flush_quads(GLuint program, bool blended) {
    if (g_vertices.empty()) return;

    glUseProgram(program);
    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(g_vertices.size() * sizeof(float)), g_vertices.data());

    if (blended) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(g_vertices.size() / 4));

    if (blended) glDisable(GL_BLEND);
    glBindVertexArray(0);
    glUseProgram(0);

    g_frame_stats.draw_calls++;
    g_frame_stats.vertices += static_cast<int>(g_vertices.size() / 4);
    g_vertices.clear();
}

void collect_drawables(const Node* node, std::vector<const Node*>& out) {
    out.push_back(node);
    for (const Node* child : node->children) {
        collect_drawables(child, out);
    }
}

}  // namespace

bool init_renderer(const std::string& font_path) {
    if (g_ready) return true;

    std::string path = font_path;
    if (path.empty()) path = "/usr/share/fonts/TTF/DejaVuSans.ttf";

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::fprintf(stderr, "could not open font '%s'\n", path.c_str());
        return false;
    }
    std::vector<unsigned char> data((std::istreambuf_iterator<char>(file)),
                                     std::istreambuf_iterator<char>());
    file.close();
    if (data.empty()) {
        std::fprintf(stderr, "font '%s' is empty\n", path.c_str());
        return false;
    }

    g_font_data = static_cast<unsigned char*>(std::malloc(data.size()));
    std::memcpy(g_font_data, data.data(), data.size());

    int offset = stbtt_GetFontOffsetForIndex(g_font_data, 0);
    if (offset < 0 || !stbtt_InitFont(&g_font, g_font_data, offset)) {
        std::fprintf(stderr, "stbtt_InitFont failed for '%s'\n", path.c_str());
        std::free(g_font_data);
        g_font_data = nullptr;
        return false;
    }

    bake_atlas();

    g_program = build_program(kVertexSource, kFragmentSource);
    if (!g_program) {
        std::free(g_font_data);
        g_font_data = nullptr;
        return false;
    }
    g_u_proj = glGetUniformLocation(g_program, "u_proj");
    g_u_atlas = glGetUniformLocation(g_program, "u_atlas");
    g_u_color = glGetUniformLocation(g_program, "u_color");

    g_shape_program = build_program(kShapeVertexSource, kShapeFragmentSource);
    if (!g_shape_program) {
        std::free(g_font_data);
        g_font_data = nullptr;
        return false;
    }
    g_shape_u_proj = glGetUniformLocation(g_shape_program, "u_proj");
    g_shape_u_color = glGetUniformLocation(g_shape_program, "u_color");

    glGenTextures(1, &g_texture);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    // rows are not 4 byte aligned, without this the atlas comes out sheared
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, kAtlasSize, kAtlasSize, 0, GL_RED, GL_UNSIGNED_BYTE,
                 g_atlas.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenVertexArrays(1, &g_vao);
    glGenBuffers(1, &g_vbo);
    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(kMaxGlyphsPerFrame * 6 * 4 * sizeof(float)), nullptr,
                 GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(2 * sizeof(float)));
    glBindVertexArray(0);

    g_vertices.reserve(static_cast<size_t>(kMaxGlyphsPerFrame) * 6 * 4);
    g_ready = true;
    return true;
}

void shutdown_renderer() {
    if (g_vao) glDeleteVertexArrays(1, &g_vao);
    if (g_vbo) glDeleteBuffers(1, &g_vbo);
    if (g_texture) glDeleteTextures(1, &g_texture);
    if (g_program) glDeleteProgram(g_program);
    if (g_shape_program) glDeleteProgram(g_shape_program);
    g_vao = g_vbo = g_texture = g_program = g_shape_program = 0;
    g_shape_u_proj = g_shape_u_color = -1;
    std::memset(g_glyphs, 0, sizeof(g_glyphs));
    g_ascent = g_descent = 0.0f;
    g_atlas.clear();
    g_atlas.shrink_to_fit();
    if (g_font_data) {
        std::free(g_font_data);
        g_font_data = nullptr;
    }
    g_ready = false;
}

float measure_text(const std::string& text, float size) {
    float scale = size / kAtlasFontPixels;
    float width = 0.0f;
    for (char ch : text) {
        width += g_glyphs[static_cast<unsigned char>(ch)].advance;
    }
    return width * scale;
}

// x/y are the centre of the text block, not its top left corner
// draws text with its left edge at x, vertically centred on y
void draw_text_left(const std::string& text, float x, float y, float size, Vec3 color) {
    draw_text(text, x + measure_text(text, size) * 0.5f, y, size, color);
}

void draw_text(const std::string& text, float x, float y, float size, Vec3 color) {
    if (!g_ready || text.empty()) return;

    float scale = size / kAtlasFontPixels;
    float pen = x - measure_text(text, size) * 0.5f;
    float baseline = y + g_ascent * scale - (g_ascent + g_descent) * scale * 0.5f;

    for (char ch : text) {
        const Glyph& g = g_glyphs[static_cast<unsigned char>(ch)];
        if (g.w > 0.0f && g.h > 0.0f) {
            float left = pen + g.xoff * scale;
            float top = baseline + g.yoff * scale;
            push_quad(left, top, left + g.w * scale, top + g.h * scale, g.u0, g.v0, g.u1, g.v1);
        }
        pen += g.advance * scale;
    }
    if (g_vertices.empty()) return;

    glUseProgram(g_program);
    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);

    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(g_vertices.size() * sizeof(float)), g_vertices.data());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glUniform1i(g_u_atlas, 0);
    glUniform3f(g_u_color, color.x, color.y, color.z);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(g_vertices.size() / 4));

    glDisable(GL_BLEND);
    glBindVertexArray(0);
    glUseProgram(0);

    g_frame_stats.draw_calls++;
    g_frame_stats.vertices += static_cast<int>(g_vertices.size() / 4);
    g_vertices.clear();
}

void draw_rect(float x, float y, float w, float h, Vec4 color) {
    if (!g_ready || w <= 0.0f || h <= 0.0f) return;

    push_quad(x, y, x + w, y + h, 0.0f, 0.0f, 0.0f, 0.0f);

    glUseProgram(g_shape_program);
    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(g_vertices.size() * sizeof(float)), g_vertices.data());
    glUniform4f(g_shape_u_color, color.x, color.y, color.z, color.w);

    // the shape program is separate from the text one, so it needs its own
    // projection or every quad lands outside clip space
    float proj[16];
    ortho_top_down(g_viewport[0], g_viewport[1], proj);
    glUniformMatrix4fv(g_shape_u_proj, 1, GL_FALSE, proj);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisable(GL_BLEND);

    glBindVertexArray(0);
    glUseProgram(0);

    g_frame_stats.draw_calls++;
    g_frame_stats.vertices += 6;
    g_vertices.clear();
}

void render_scene(const Scene& scene, int viewport_width, int viewport_height) {
    g_frame_stats = {};
    if (!g_ready || viewport_width <= 0 || viewport_height <= 0) return;

    g_viewport[0] = static_cast<float>(viewport_width);
    g_viewport[1] = static_cast<float>(viewport_height);

    Node* entry = scene.main_node();
    if (!entry) return;

    std::vector<const Node*> drawables;
    collect_drawables(entry, drawables);

    // stable so siblings keep their file order when z_index ties
    std::stable_sort(drawables.begin(), drawables.end(),
                     [](const Node* a, const Node* b) { return a->z_index < b->z_index; });

    float proj[16];
    ortho_top_down(static_cast<float>(viewport_width), static_cast<float>(viewport_height), proj);
    glUseProgram(g_program);
    glUniformMatrix4fv(g_u_proj, 1, GL_FALSE, proj);

    for (const Node* node : drawables) {
        if (!node->visible || !node->is_text()) continue;

        Vec2 px = node->to_pixels(static_cast<float>(viewport_width),
                                  static_cast<float>(viewport_height));
        draw_text(node->text, px.x, px.y, node->font_size, node->text_color);
    }
    glUseProgram(0);
}

void set_background(Vec4 color) { g_background = color; }

Vec4 background() { return g_background; }

FrameStats last_frame_stats() { return g_frame_stats; }

}  // namespace grace