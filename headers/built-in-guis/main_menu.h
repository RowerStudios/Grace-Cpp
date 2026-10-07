#pragma once

// the main menu, real qt widgets this time. one tab per section, settings page
// writes straight into app_settings and asks the window to re-apply them.

#include <QWidget>

#include "app_settings.h"

class QCheckBox;
class QComboBox;
class QPushButton;
class QSpinBox;

namespace grace {
namespace gui {

class MainMenu : public QWidget {
    Q_OBJECT

public:
    explicit MainMenu(QWidget* parent = nullptr);

signals:
    // settings changed, the window has to push them into the screen
    void settingsChanged();
    // the player wants to start the game
    void playPressed();

private:
    QWidget* buildPlayersPage();
    QWidget* buildSettingsPage();
    QWidget* buildControlsPage();
    QWidget* buildAboutPage();

    // reads the widgets into the staged copy, nothing reaches the window yet
    void stageSettings();
    // copies the staged copy over the live settings and tells the window
    void commitSettings();

    // what the widgets edit. only becomes real on apply.
    AppSettings m_staged;
    QPushButton* m_apply = nullptr;

    QCheckBox* m_vsync = nullptr;
    QSpinBox* m_max_fps = nullptr;
    QComboBox* m_window_mode = nullptr;
    QComboBox* m_window_scale = nullptr;
    QSpinBox* m_custom_w = nullptr;
    QSpinBox* m_custom_h = nullptr;
};

}  // namespace gui
}  // namespace grace