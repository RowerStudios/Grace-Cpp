#include <QApplication>
#include <QMainWindow>
#include <QScreen>
#include <QShortcut>
#include <QStackedWidget>
#include <QSurfaceFormat>

#include "app_settings.h"
#include "built-in-guis/main_menu.h"
#include "game_widget.h"

namespace {

// the same dark look the hand rolled ui had, now as a stylesheet
const char* kStyle = R"(
QWidget { background: #14161b; color: #e4e7ef; font-size: 14px; }
QTabWidget::pane { border: none; background: #14161b; }
QTabBar::tab {
    background: transparent; color: #8b90a0; padding: 14px 26px;
    border: none; font-size: 16px;
}
QTabBar::tab:selected { color: #f2f5fb; }
QTabBar::tab:hover { color: #d3d8e6; }
QLabel#heading { font-size: 20px; font-weight: 600; }
QLabel#hint { color: #6d7385; font-size: 13px; }
QLabel#keyCap {
    background: #22262f; border: 1px solid #2f3440; border-radius: 4px;
    padding: 3px 10px;
}
QFrame#separator { background: #262a34; max-height: 1px; border: none; }
QCheckBox { spacing: 10px; }
QCheckBox::indicator {
    width: 18px; height: 18px; border-radius: 3px;
    border: 1px solid #3a4050; background: #1b1e26;
}
QCheckBox::indicator:checked { background: #f2f5fb; border-color: #f2f5fb; }
QComboBox, QSpinBox {
    background: #1b1e26; border: 1px solid #2f3440; border-radius: 4px;
    padding: 6px 10px; min-width: 180px;
}
QComboBox:hover, QSpinBox:hover { border-color: #f2f5fb; }
QComboBox::drop-down { border: none; width: 20px; }
QComboBox QAbstractItemView {
    background: #1b1e26; border: 1px solid #2f3440; selection-background-color: #f2f5fb; selection-color: #14161b;
}
QPushButton {
    background: #f2f5fb; color: #14161b; border: none; border-radius: 5px;
    padding: 13px; font-size: 15px; font-weight: 600;
}
QPushButton:hover { background: #ffffff; }
QPushButton:pressed { background: #d3d9e5; }
QPushButton#applyButton {
    background: #2f3642; color: #e4e7ef; padding: 9px 26px; font-weight: 500;
}
QPushButton#applyButton:hover { background: #3a4250; }
QPushButton#applyButton:pressed { background: #262c36; }
QPushButton#applyButton:disabled { background: #22262e; color: #565c6b; }
)";

class MainWindow : public QMainWindow {
public:
    MainWindow() {
        setWindowTitle("Grace Cpp");
        setStyleSheet(kStyle);

        auto* stack = new QStackedWidget(this);
        auto* game = new grace::GameWidget;
        auto* menu = new grace::gui::MainMenu;
        stack->addWidget(game);
        stack->addWidget(menu);
        setCentralWidget(stack);

        m_game = game;
        m_menu = menu;
        auto goToMenu = [this, stack] { stack->setCurrentWidget(m_menu); };
        auto goToGame = [this, stack] {
            stack->setCurrentWidget(m_game);
            m_game->setFocus();
        };

        connect(game, &grace::GameWidget::escapePressed, this, goToMenu);
        connect(menu, &grace::gui::MainMenu::playPressed, this, goToGame);

        // esc inside the menu goes back to the game
        auto* esc = new QShortcut(QKeySequence(Qt::Key_Escape), menu);
        esc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(esc, &QShortcut::activated, this, goToGame);

        connect(menu, &grace::gui::MainMenu::settingsChanged, this,
                [this] { applyWindowSettings(); });

        stack->setCurrentWidget(menu);  // boot into the menu
        applyWindowSettings();
    }

private:
    void applyWindowSettings() {
        const grace::AppSettings& s = grace::app_settings();

        // frame pacing is cheap and safe to re-apply, so it always runs
        if (m_game) m_game->applySettings();

        // only touch the window when something it actually cares about moved.
        // setwindowflags and showfullscreen recreate the native window, which
        // destroys the opengl context and re-runs initializegl, so doing that
        // on every vsync tweak would stall or kill the renderer
        bool size_changed = s.window_mode != m_applied_mode ||
                            s.window_scale != m_applied_scale ||
                            s.custom_width != m_applied_width ||
                            s.custom_height != m_applied_height;
        if (!size_changed) return;

        Qt::WindowFlags flags = Qt::Window;
        if (s.window_mode == grace::WindowMode::Borderless) flags |= Qt::FramelessWindowHint;
        if (flags != windowFlags()) setWindowFlags(flags);

        if (s.window_mode == grace::WindowMode::Fullscreen) {
            showFullScreen();
        } else if (isFullScreen()) {
            showNormal();
        }

        switch (s.window_scale) {
            case grace::WindowScale::Resizable:
                // clearing a fixed size means lifting both bounds, not setting
                // them to zero, a zero maximum clips every resize away
                setMinimumSize(0, 0);
                setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
                resize(s.custom_width, s.custom_height);
                break;
            case grace::WindowScale::Custom:
                setFixedSize(s.custom_width, s.custom_height);
                break;
            case grace::WindowScale::Scaled:
                if (QScreen* target = screen()) {
                    // fills the screen and cannot be dragged smaller
                    setFixedSize(target->geometry().size());
                }
                break;
        }

        m_applied_mode = s.window_mode;
        m_applied_scale = s.window_scale;
        m_applied_width = s.custom_width;
        m_applied_height = s.custom_height;
    }

    grace::GameWidget* m_game = nullptr;
    grace::gui::MainMenu* m_menu = nullptr;
    grace::WindowMode m_applied_mode = static_cast<grace::WindowMode>(-1);
    grace::WindowScale m_applied_scale = static_cast<grace::WindowScale>(-1);
    int m_applied_width = -1;
    int m_applied_height = -1;
};

}  // namespace

int main(int argc, char* argv[]) {
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setSwapInterval(grace::app_settings().vsync ? 1 : 0);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);
    app.setApplicationName("Grace Cpp");
    app.setOrganizationName("RowerStudios");

    MainWindow window;
    window.show();  // applyWindowSettings already picked the size

    return app.exec();
}