#include "game_widget.h"

#include <QDebug>
#include <QKeyEvent>
#include <QOpenGLContext>

#include <algorithm>

#include <renderer.h>
#include <scene_loader.h>
#include <sound.h>

namespace grace {
namespace {

// glad wants a plain function pointer, and it needs to reach the current
// context, so stash it here for the duration of the load
QOpenGLContext* s_resolve_context = nullptr;

GLADapiproc resolve_gl_symbol(const char* name) {
    if (!s_resolve_context) return nullptr;
    return reinterpret_cast<GLADapiproc>(s_resolve_context->getProcAddress(name));
}

}  // namespace

GameWidget::GameWidget(QWidget* parent) : QOpenGLWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(320, 240);

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &GameWidget::onTick);

    // both clocks start here so the first frame never measures against an
    // unstarted timer
    m_frame_clock.start();
    m_fps_window.start();

    applySettings();
}

GameWidget::~GameWidget() {
    // ~qopenglwidget makes the context current after this runs, so any gl
    // cleanup has to happen here with an explicit makecurrent
    if (m_renderer_live) {
        makeCurrent();
        shutdown_renderer();
        m_renderer_live = false;
    }
}

void GameWidget::applySettings() {
    m_settings = &app_settings();

    // with vsync the driver already paces us to the display, so the timer is
    // just a pump. without it the timer is the cap
    if (m_settings->vsync) {
        m_frame_budget_ms = 16;
    } else {
        int fps = std::clamp(m_settings->max_fps, 1, 1000);
        m_frame_budget_ms = std::max(1, 1000 / fps);
    }
    m_timer->start(m_frame_budget_ms);

    // the swap interval is a gl call and the context is only current inside
    // paintgl, so it gets deferred to the next frame instead of run here
    m_swap_interval_dirty = true;
}

// qt6 has no runtime swap interval api, the platform extension is the way.
// must run with the context current, so only from paintgl.
void GameWidget::applySwapInterval() {
    QOpenGLContext* ctx = context();
    if (!ctx || !ctx->isValid()) return;

    int interval = m_settings && m_settings->vsync ? 1 : 0;

    // mesa: glXSwapIntervalMESA(int), windows: wglSwapIntervalEXT(int)
    using SwapIntervalFn = void (*)(int);
    if (auto fn = reinterpret_cast<SwapIntervalFn>(ctx->getProcAddress("glXSwapIntervalMESA"))) {
        fn(interval);
        return;
    }
    if (auto fn = reinterpret_cast<SwapIntervalFn>(ctx->getProcAddress("wglSwapIntervalEXT"))) {
        fn(interval);
    }
}

void GameWidget::initializeGL() {
    // audio is independent of the gl context but the device opens once for the
    // whole app, so check before trying
    if (!m_sound.is_open()) {
        std::string reason;
        if (!m_sound.open(&reason)) {
            qWarning("audio unavailable: %s", reason.c_str());
        }
    }
    // glad needs an address resolver, qtopenglcontext hands one over
    s_resolve_context = context();
    if (!s_resolve_context) return;

    if (!gladLoadGL(&resolve_gl_symbol)) {
        return;
    }

    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    m_gl_version = version ? QString::fromLatin1(version) : QStringLiteral("?");
    m_gl_renderer = renderer ? QString::fromLatin1(renderer) : QStringLiteral("?");

    init_renderer();
    m_renderer_live = true;
    m_context_ready = true;

    // reparenting the widget to a different top level destroys the context and
    // calls initializegl again, so gl objects have to go when it is announced
    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, [this] {
        if (m_renderer_live) {
            makeCurrent();
            shutdown_renderer();
            m_renderer_live = false;
        }
        m_context_ready = false;
    });

    try {
        change_scene_to("main");
        m_scene_ready = true;
        validate_scene_sounds(current_scene());
        m_sound.load_scene(current_scene());
    } catch (const std::exception&) {
        // a broken scene should still leave a running window with the overlay
        m_scene_ready = false;
    }

    applySettings();
}

void GameWidget::resizeGL(int w, int h) {
    m_viewport_w = w;
    m_viewport_h = h;
    glViewport(0, 0, w, h);
}

void GameWidget::onTick() { update(); }

void GameWidget::paintGL() {
    m_frame_ms = static_cast<float>(m_frame_clock.restart());

    if (m_swap_interval_dirty) {
        m_swap_interval_dirty = false;
        applySwapInterval();
    }

    // scaled mode draws at a fixed size and the widget stretches it to the
    // screen, everything else draws at the real pixel size
    int render_w = m_viewport_w;
    int render_h = m_viewport_h;
    if (m_settings->window_scale == WindowScale::Scaled) {
        render_w = m_settings->render_width();
        render_h = m_settings->render_height();
    }

    // black unless a scene overrides it, so an empty 2d scene is just black
    Vec4 clear = background();
    glClearColor(clear.x, clear.y, clear.z, clear.w);
    glClear(GL_COLOR_BUFFER_BIT);

    if (m_scene_ready) {
        render_scene(current_scene(), render_w, render_h);
    }

    if (m_debug_on) {
        Scene& scene = current_scene();
        FrameStats stats = last_frame_stats();

        gui::DebugInfo info;
        info.fps = m_fps;
        info.frame_ms = m_frame_ms;
        info.scene = scene.name;
        info.scene_source = scene_manager().path_for(scene.name);
        Node* entry = scene.main_node();
        info.scene_entry = entry ? entry->name : "(none)";
        info.scene_nodes = static_cast<size_t>(scene.nodes.size());
        info.scene_mains = static_cast<int>(scene.main_count());
        info.window_w = m_viewport_w;
        info.window_h = m_viewport_h;
        info.pixel_w = render_w;
        info.pixel_h = render_h;
        info.draw_calls = stats.draw_calls;
        info.vertices = stats.vertices;
        info.gl_version = m_gl_version.toStdString();
        info.gl_renderer = m_gl_renderer.toStdString();
        gui::draw_debug_panel(info);
    }

    // averaged over half a second so the readout does not jitter per frame
    m_frame_count++;
    qint64 window_ms = m_fps_window.elapsed();
    if (window_ms >= 500) {
        m_fps = static_cast<float>(m_frame_count) * 1000.0f / static_cast<float>(window_ms);
        m_frame_count = 0;
        m_fps_window.restart();
    }
}

void GameWidget::reloadScene() {
    try {
        // change_scene_to only swaps the live scene in once the new one parsed
        // and has exactly one main node, so a broken file leaves the old scene
        // running
        change_scene_to("main");
        m_scene_ready = true;
        validate_scene_sounds(current_scene());
        m_sound.load_scene(current_scene());
    } catch (const std::exception& error) {
        qWarning("scene reload failed: %s", error.what());
    }
}

void GameWidget::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_Escape:
            emit escapePressed();
            return;
        case Qt::Key_F3:
            m_debug_on = !m_debug_on;
            return;
        case Qt::Key_F5:
            reloadScene();
            return;
        default:
            break;
    }
    QOpenGLWidget::keyPressEvent(event);
}

}  // namespace grace