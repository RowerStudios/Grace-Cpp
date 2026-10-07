// glad before qt, qt drags in <GL/gl.h> which glad will not share a tu with
#include <glad/gl.h>
#include "main_menu.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace grace {
namespace gui {
namespace {
constexpr int kLabelColumnWidth = 190;

QLabel* hint(const QString& text) {
    QLabel* label = new QLabel(text);
    label->setObjectName("hint");
    label->setWordWrap(true);
    return label;
}
QLabel* heading(const QString& text) {
    QLabel* label = new QLabel(text);
    label->setObjectName("heading");
    return label;
}
QFrame* separator() {
    QFrame* line = new QFrame();
    line->setObjectName("separator");
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Plain);
    return line;
}
// pages go in a scroll area so a wide page cannot force a minimum window size
QScrollArea* scrollable(QWidget* page) {
    auto* area = new QScrollArea;
    area->setWidget(page);
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    area->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    return area;
}
// labels get their own fixed width column so every control starts at the same
// x and no label ever ends up hugging its box
QLabel* row_label(const QString& text) {
    auto* label = new QLabel(text);
    label->setFixedWidth(kLabelColumnWidth);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    return label;
}
}  // namespace
MainMenu::MainMenu(QWidget* parent) : QWidget(parent) {
    m_staged = app_settings();
    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    QTabWidget* tabs = new QTabWidget(this);
    tabs->setObjectName("tabs");
    tabs->setDocumentMode(true);
    tabs->addTab(scrollable(buildPlayersPage()), "Players");
    tabs->addTab(scrollable(buildSettingsPage()), "Settings");
    tabs->addTab(scrollable(buildControlsPage()), "Controls");
    tabs->addTab(scrollable(buildAboutPage()), "About");
    outer->addWidget(tabs);
    QPushButton* play = new QPushButton("Play");
    play->setObjectName("playButton");
    connect(play, &QPushButton::clicked, this, &MainMenu::playPressed);
    outer->addWidget(play);
}
QWidget* MainMenu::buildPlayersPage() {
    QWidget* page = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);
    layout->addWidget(heading("Players"));
    layout->addWidget(separator());
    layout->addWidget(hint("no players connected"));
    layout->addStretch(1);
    return page;
}
QWidget* MainMenu::buildSettingsPage() {
    QWidget* page = new QWidget;
    QVBoxLayout* page_layout = new QVBoxLayout(page);
    page_layout->setContentsMargins(28, 24, 28, 24);
    page_layout->setSpacing(14);
    page_layout->addWidget(heading("Display"));
    page_layout->addWidget(separator());
    QFormLayout* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFormAlignment(Qt::AlignTop);
    form->setHorizontalSpacing(24);
    form->setVerticalSpacing(14);
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    m_vsync = new QCheckBox;
    m_vsync->setChecked(m_staged.vsync);
    m_vsync->setToolTip("Sync frames to the display refresh rate");
    connect(m_vsync, &QCheckBox::toggled, this, &MainMenu::stageSettings);
    form->addRow(row_label("VSync"), m_vsync);
    m_max_fps = new QSpinBox;
    m_max_fps->setRange(1, 1000);
    m_max_fps->setValue(m_staged.max_fps);
    connect(m_max_fps, &QSpinBox::valueChanged, this, &MainMenu::stageSettings);
    // the unit sits beside the box, not inside it
    QWidget* fps_row = new QWidget;
    QHBoxLayout* fps_layout = new QHBoxLayout(fps_row);
    fps_layout->setContentsMargins(0, 0, 0, 0);
    fps_layout->setSpacing(8);
    fps_layout->addWidget(m_max_fps);
    fps_layout->addWidget(new QLabel("fps"));
    fps_layout->addStretch(1);
    form->addRow(row_label("Max FPS"), fps_row);
    m_window_mode = new QComboBox;
    m_window_mode->addItems({"Windowed", "Fullscreen", "Borderless"});
    m_window_mode->setCurrentIndex(static_cast<int>(m_staged.window_mode));
    connect(m_window_mode, &QComboBox::currentIndexChanged, this, &MainMenu::stageSettings);
    form->addRow(row_label("Window Mode"), m_window_mode);
    m_window_scale = new QComboBox;
    m_window_scale->addItems({"Resizable", "Custom", "Scaled"});
    m_window_scale->setCurrentIndex(static_cast<int>(m_staged.window_scale));
    connect(m_window_scale, &QComboBox::currentIndexChanged, this, &MainMenu::stageSettings);
    form->addRow(row_label("Window Scale"), m_window_scale);
    m_custom_w = new QSpinBox;
    m_custom_w->setRange(320, 7680);
    m_custom_w->setSingleStep(20);
    m_custom_w->setValue(m_staged.custom_width);
    connect(m_custom_w, &QSpinBox::valueChanged, this, &MainMenu::stageSettings);
    m_custom_h = new QSpinBox;
    m_custom_h->setRange(240, 4320);
    m_custom_h->setSingleStep(20);
    m_custom_h->setValue(m_staged.custom_height);
    connect(m_custom_h, &QSpinBox::valueChanged, this, &MainMenu::stageSettings);
    // the size boxes only make sense in custom mode
    QWidget* size_row = new QWidget;
    QHBoxLayout* size_layout = new QHBoxLayout(size_row);
    size_layout->setContentsMargins(0, 0, 0, 0);
    size_layout->setSpacing(10);
    size_layout->addWidget(m_custom_w);
    size_layout->addWidget(new QLabel("x"));
    size_layout->addWidget(m_custom_h);
    size_layout->addWidget(new QLabel("px"));
    size_layout->addStretch(1);
    form->addRow(row_label("Custom size"), size_row);
    page_layout->addLayout(form);
    page_layout->addWidget(hint("Custom locks the window to an exact size, "
                               "Scaled fills the screen and renders at 1920x1080. "
                               "Nothing changes until you press apply."));
    page_layout->addStretch(1);
    m_apply = new QPushButton("Apply");
    m_apply->setObjectName("applyButton");
    connect(m_apply, &QPushButton::clicked, this, &MainMenu::commitSettings);
    page_layout->addWidget(m_apply, 0, Qt::AlignRight);
    // reflect the saved scale mode and leave apply greyed out
    stageSettings();
    return page;
}
QWidget* MainMenu::buildControlsPage() {
    QWidget* page = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);
    layout->addWidget(heading("Controls"));
    layout->addWidget(separator());
    struct Binding {
        const char* action;
        const char* key;
    };
    const Binding bindings[] = { // for future use
        {"Move forward", "W"},
        {"Move back", "S"},
        {"Move left", "A"},
        {"Move right", "D"},
        {"Jump", "Space"},
        {"Open / close menu", "Esc"},
        {"Toggle debug overlay", "F3"},
        {"Reload scene", "F5"},
    };
    QFormLayout* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setHorizontalSpacing(24);
    form->setVerticalSpacing(10);
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    for (const Binding& binding : bindings) {
        QLabel* key = new QLabel(QString::fromLatin1(binding.key));
        key->setObjectName("keyCap");
        key->setFixedWidth(160);
        key->setAlignment(Qt::AlignCenter);
        form->addRow(row_label(QString::fromLatin1(binding.action)), key);
    }
    // keeps the key caps at their own width instead of filling the row
    form->addRow(new QWidget, new QWidget);
    layout->addLayout(form);
    layout->addWidget(hint("Rebinding arrives with the player controller."));
    layout->addStretch(1);
    return page;
}
QWidget* MainMenu::buildAboutPage() {
    QWidget* page = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(12);
    layout->addWidget(heading("Grace Cpp"));
    layout->addWidget(hint("A recreation of the Roblox game Grace, written in C++ "
                           "with Qt, OpenGL 3.3 core and SDL-free windowing."));
    layout->addWidget(separator());
    QFormLayout* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setHorizontalSpacing(24);
    form->setVerticalSpacing(8);
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    form->addRow(row_label("Scene format"), new QLabel(".gscn"));
    form->addRow(row_label("Qt"), new QLabel(QString::fromLatin1(qVersion())));
    form->addRow(row_label("Licence"), new QLabel("GNU GPL v3"));
    form->addRow(new QWidget, new QWidget);
    layout->addLayout(form);
    layout->addStretch(1);
    return page;
}

void MainMenu::stageSettings() {
    m_staged.vsync = m_vsync->isChecked();
    m_staged.max_fps = m_max_fps->value();
    m_staged.window_mode = static_cast<WindowMode>(m_window_mode->currentIndex());
    m_staged.window_scale = static_cast<WindowScale>(m_window_scale->currentIndex());
    m_staged.custom_width = m_custom_w->value();
    m_staged.custom_height = m_custom_h->value();
    bool custom = m_staged.window_scale == WindowScale::Custom;
    m_custom_w->setEnabled(custom);
    m_custom_h->setEnabled(custom);
    // only offer apply once something actually differs from what is running
    m_apply->setEnabled(m_staged != app_settings());
}

void MainMenu::commitSettings() {
    if (m_staged == app_settings()) return;
    app_settings() = m_staged;
    app_settings().save();
    m_apply->setEnabled(false);
    emit settingsChanged();
}
}  // namespace gui
}  // namespace grace
