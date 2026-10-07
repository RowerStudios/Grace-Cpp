#pragma once

// the gl surface. qt owns the window and the context, our renderer draws the
// scene into it, and qtiming drives the frames.

// glad has to be included before any qt header: qt pulls in the system
// <GL/gl.h>, and glad refuses to sit next to it
#include <glad/gl.h>

#include <QElapsedTimer>
#include <QOpenGLWidget>
#include <QString>
#include <QTimer>

#include "app_settings.h"
#include "debug_panel.h"
#include "sound.h"

namespace grace {

class GameWidget : public QOpenGLWidget {
    Q_OBJECT

public:
    explicit GameWidget(QWidget* parent = nullptr);
    ~GameWidget() override;

    // re-reads the settings and retunes the frame pacing. call this whenever
    // the menu changes something, otherwise the widget keeps stale values
    void applySettings();

// re-reads the scene file from disk. the running scene is kept if the new one
// fails to parse, so a bad edit cannot blank the game
void reloadScene();

signals:
    void escapePressed();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void onTick();
    void applySwapInterval();

    // points at the shared settings, never a copy, so live changes are seen
    AppSettings* m_settings = nullptr;
    QTimer* m_timer = nullptr;
    int m_frame_budget_ms = 16;

    QElapsedTimer m_frame_clock;
    QElapsedTimer m_fps_window;
    int m_frame_count = 0;
    float m_fps = 0.0f;
    float m_frame_ms = 0.0f;

    int m_viewport_w = 0;
    int m_viewport_h = 0;
    bool m_debug_on = false;
    bool m_scene_ready = false;
    bool m_context_ready = false;
    bool m_renderer_live = false;
    bool m_swap_interval_dirty = false;
    QString m_gl_version;
    QString m_gl_renderer;
    SoundPlayer m_sound;
};

}  // namespace grace