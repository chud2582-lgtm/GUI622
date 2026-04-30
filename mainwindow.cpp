#include "mainwindow.h"

#include "robotpreviewwidget.h"
#include "autocalibrationwidget.h"

#include "cpp/interface/nrc_interface.h"
#include "cpp/interface/nrc_io.h"
#include "cpp/interface/nrc_job_operate.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QDateTime>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QMessageBox>
#include <QScreen>
#include <QTextCursor>
#include <QVBoxLayout>
#include <QStyle>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

constexpr int kVisibleAxisCount = 6;
constexpr int kInternalAxisCount = 7;

void clearHiddenAxis(std::array<double, 7>& pose)
{
    pose[6] = 0.0;
}

QString formatVisiblePose(const std::array<double, 7>& pose)
{
    QStringList values;
    values.reserve(kVisibleAxisCount);
    for (int i = 0; i < kVisibleAxisCount; ++i) {
        values << QString::number(pose[i], 'f', 3);
    }
    return values.join(QStringLiteral(", "));
}

QString servoStateText(int state)
{
    switch (state) {
    case 0:
        return QStringLiteral("停止");
    case 1:
        return QStringLiteral("就绪");
    case 2:
        return QStringLiteral("报警");
    case 3:
        return QStringLiteral("运行");
    default:
        return QStringLiteral("--");
    }
}

QString runStateText(int state)
{
    switch (state) {
    case 0:
        return QStringLiteral("停止");
    case 1:
        return QStringLiteral("暂停");
    case 2:
        return QStringLiteral("运行中");
    default:
        return QStringLiteral("--");
    }
}

double stepTowards(double current, double target, double step)
{
    if (std::abs(target - current) <= step) {
        return target;
    }
    return current + (target > current ? step : -step);
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("英联机械臂控制界面 - NRC Robot Controller"));

    if (QScreen *screen = QApplication::primaryScreen()) {
        const QRect screenRect = screen->availableGeometry();
        resize(qMin(1500, screenRect.width() - 120), qMin(960, screenRect.height() - 120));
    } else {
        resize(1400, 900);
    }

    setupUI();
    setupStyleSheet();

    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(150);
    connect(m_updateTimer, &QTimer::timeout, this, &MainWindow::onTimerUpdate);

    m_previewTimer = new QTimer(this);
    m_previewTimer->setInterval(33);
    connect(m_previewTimer, &QTimer::timeout, this, &MainWindow::onPreviewFrame);

    resetDemoState();
    refreshPositionHeaders();
    refreshPositionDisplay();
    refreshPreview(false);
    refreshJobStatus();
    refreshStatusSummary();

    appendLog(QStringLiteral("系统初始化完成，可直接连接控制器；未连接成功时会自动进入演示预览模式。"));
    if (m_logEdit) {
        m_logEdit->clear();
        appendLog(QStringLiteral("系统初始化完成，可直接连接控制器；未连接成功时会自动进入演示预览模式。"));
    }
}

MainWindow::~MainWindow()
{
    stopTimers();

    if (m_connected && !m_demoMode && m_socketFd != -1) {
        disconnect_robot(m_socketFd);
    }
}

void MainWindow::setupStyleSheet()
{
    const QString style = QString::fromUtf8(R"(
        QMainWindow {
            background-color: #1e1e2e;
        }
        QWidget {
            color: #cdd6f4;
            font-family: "Microsoft YaHei", "SimHei", sans-serif;
        }
        QGroupBox {
            border: 1px solid #45475a;
            border-radius: 6px;
            margin-top: 12px;
            padding-top: 14px;
            font-weight: bold;
            font-size: 12px;
            background-color: #181825;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 6px;
            color: #89b4fa;
        }
        QTabWidget::pane {
            border: 1px solid #45475a;
            border-radius: 4px;
            background-color: #1e1e2e;
        }
        QTabBar::tab {
            background-color: #313244;
            color: #a6adc8;
            padding: 8px 20px;
            margin-right: 2px;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            font-size: 12px;
        }
        QTabBar::tab:selected {
            background-color: #45475a;
            color: #89b4fa;
            font-weight: bold;
        }
        QTabBar::tab:hover {
            background-color: #585b70;
        }
        QPushButton {
            background-color: #313244;
            color: #cdd6f4;
            border: 1px solid #45475a;
            border-radius: 5px;
            padding: 6px 16px;
            font-size: 12px;
            min-height: 28px;
        }
        QPushButton:hover {
            background-color: #45475a;
            border-color: #89b4fa;
        }
        QPushButton:pressed {
            background-color: #585b70;
        }
        QPushButton:disabled {
            background-color: #1e1e2e;
            color: #585b70;
            border-color: #313244;
        }
        QPushButton[styleClass="success"] {
            background-color: #1e4620;
            border-color: #a6e3a1;
            color: #a6e3a1;
        }
        QPushButton[styleClass="success"]:hover {
            background-color: #2d6930;
        }
        QPushButton[styleClass="danger"] {
            background-color: #4e1e1e;
            border-color: #f38ba8;
            color: #f38ba8;
        }
        QPushButton[styleClass="danger"]:hover {
            background-color: #6e2e2e;
        }
        QPushButton[styleClass="warning"] {
            background-color: #4e3e1e;
            border-color: #f9e2af;
            color: #f9e2af;
        }
        QPushButton[styleClass="warning"]:hover {
            background-color: #6e5e2e;
        }
        QPushButton[styleClass="primary"] {
            background-color: #1e3a5f;
            border-color: #89b4fa;
            color: #89b4fa;
        }
        QPushButton[styleClass="primary"]:hover {
            background-color: #2e4a7f;
        }
        QPushButton[styleClass="jog"] {
            background-color: #313244;
            border: 1px solid #585b70;
            border-radius: 4px;
            padding: 4px 8px;
            font-size: 14px;
            font-weight: bold;
            min-width: 36px;
            min-height: 32px;
        }
        QPushButton[styleClass="jog"]:pressed {
            background-color: #89b4fa;
            color: #1e1e2e;
        }
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            background-color: #313244;
            color: #cdd6f4;
            border: 1px solid #45475a;
            border-radius: 4px;
            padding: 4px 8px;
            min-height: 24px;
            font-size: 12px;
        }
        QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
            border-color: #89b4fa;
        }
        QComboBox::drop-down {
            border: none;
            width: 20px;
        }
        QComboBox QAbstractItemView {
            background-color: #313244;
            color: #cdd6f4;
            border: 1px solid #45475a;
            selection-background-color: #45475a;
        }
        QSlider::groove:horizontal {
            height: 6px;
            background: #313244;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: #89b4fa;
            width: 16px;
            height: 16px;
            margin: -5px 0;
            border-radius: 8px;
        }
        QSlider::sub-page:horizontal {
            background: #89b4fa;
            border-radius: 3px;
        }
        QTextEdit {
            background-color: #11111b;
            color: #a6adc8;
            border: 1px solid #313244;
            border-radius: 4px;
            font-family: "Consolas", "Courier New", monospace;
            font-size: 11px;
            padding: 4px;
        }
        QTableWidget {
            background-color: #181825;
            color: #cdd6f4;
            border: 1px solid #313244;
            gridline-color: #313244;
            font-size: 11px;
        }
        QTableWidget::item:selected {
            background-color: #45475a;
        }
        QHeaderView::section {
            background-color: #313244;
            color: #89b4fa;
            padding: 4px;
            border: 1px solid #45475a;
            font-weight: bold;
            font-size: 11px;
        }
        QLabel[styleClass="value"] {
            font-family: "Consolas", "Courier New", monospace;
            font-size: 13px;
            color: #a6e3a1;
            background-color: #11111b;
            border: 1px solid #313244;
            border-radius: 3px;
            padding: 4px 8px;
            min-width: 80px;
            qproperty-alignment: AlignCenter;
        }
        QLabel[styleClass="header"] {
            color: #bac2de;
            font-size: 11px;
            qproperty-alignment: AlignCenter;
        }
        QLabel[styleClass="status-connected"] {
            color: #a6e3a1;
            font-weight: bold;
        }
        QLabel[styleClass="status-disconnected"] {
            color: #f38ba8;
            font-weight: bold;
        }
        QLabel[styleClass="subtle"] {
            color: #bac2de;
            font-size: 11px;
        }
        QProgressBar {
            background-color: #313244;
            border: 1px solid #45475a;
            border-radius: 3px;
            text-align: center;
            color: #cdd6f4;
            font-size: 11px;
            max-height: 16px;
        }
        QProgressBar::chunk {
            background-color: #89b4fa;
            border-radius: 2px;
        }
        QMessageBox,
        QMessageBox QWidget {
            background-color: #000000;
        }
        QMessageBox QLabel {
            color: #ffffff;
            background-color: transparent;
        }
        QMessageBox QPushButton {
            background-color: #111111;
            color: #ffffff;
            border: 1px solid #666666;
            min-width: 72px;
        }
        QMessageBox QPushButton:hover {
            background-color: #1d1d1d;
            border-color: #ffffff;
        }
        QMessageBox QPushButton:pressed {
            background-color: #2a2a2a;
        }
        QStatusBar {
            background-color: #11111b;
            color: #a6adc8;
            font-size: 11px;
        }
    )");
    qApp->setStyleSheet(style);
}

void MainWindow::setupUI()
{
    QWidget *central = new QWidget(this);
    setCentralWidget(central);

    QVBoxLayout *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(8, 8, 8, 4);
    mainLayout->setSpacing(6);

    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->addWidget(createConnectionPanel(), 1);
    topLayout->addWidget(createServoPanel(), 1);
    mainLayout->addLayout(topLayout);

    QSplitter *splitter = new QSplitter(Qt::Horizontal);

    QTabWidget *tabs = new QTabWidget();
    tabs->addTab(createJogPanel(), QStringLiteral("点动控制"));
    tabs->addTab(createMotionPanel(), QStringLiteral("运动指令"));
    tabs->addTab(createJobPanel(), QStringLiteral("作业管理"));
    tabs->addTab(createAutoCalibrationPanel(), QStringLiteral("自动标定"));
    splitter->addWidget(tabs);

    QWidget *rightPanel = new QWidget();
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(6);
    rightLayout->addWidget(createPositionPanel());
    rightLayout->addWidget(createPreviewPanel(), 1);

    QGroupBox *logGroup = createStyledGroup(QStringLiteral("运行日志"));
    logGroup->setTitle(QStringLiteral("运行日志"));
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);
    m_logEdit = new QTextEdit();
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumHeight(190);
    logLayout->addWidget(m_logEdit);
    rightLayout->addWidget(logGroup);

    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    mainLayout->addWidget(splitter, 1);

    QStatusBar *sb = statusBar();
    m_robotRunStateLabel = new QLabel(QStringLiteral("运行状态: --"));
    m_robotTypeLabel = new QLabel(QStringLiteral("机器人类型: --"));
    sb->addWidget(m_robotRunStateLabel);
    sb->addPermanentWidget(m_robotTypeLabel);
}

QWidget* MainWindow::createConnectionPanel()
{
    QGroupBox *group = createStyledGroup(QStringLiteral("连接设置"));
    QGridLayout *layout = new QGridLayout(group);
    layout->setSpacing(8);

    layout->addWidget(new QLabel(QStringLiteral("IP地址:")), 0, 0);
    m_ipEdit = new QLineEdit(QStringLiteral("192.168.3.15"));
    layout->addWidget(m_ipEdit, 0, 1);

    layout->addWidget(new QLabel(QStringLiteral("端口:")), 0, 2);
    m_portEdit = new QLineEdit(QStringLiteral("6001"));
    m_portEdit->setMaximumWidth(84);
    layout->addWidget(m_portEdit, 0, 3);

    m_connectBtn = createStyledButton(QStringLiteral("连接"), QStringLiteral("success"));
    m_disconnectBtn = createStyledButton(QStringLiteral("断开"), QStringLiteral("danger"));
    m_disconnectBtn->setEnabled(false);
    layout->addWidget(m_connectBtn, 0, 4);
    layout->addWidget(m_disconnectBtn, 0, 5);

    m_connStatusLabel = new QLabel(QStringLiteral("● 未连接"));
    m_connStatusLabel->setProperty("styleClass", "status-disconnected");
    layout->addWidget(m_connStatusLabel, 1, 0, 1, 6);

    connect(m_connectBtn, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);

    return group;
}

QWidget* MainWindow::createServoPanel()
{
    QGroupBox *group = createStyledGroup(QStringLiteral("伺服控制"));
    QGridLayout *layout = new QGridLayout(group);
    layout->setSpacing(8);

    m_servoReadyBtn = createStyledButton(QStringLiteral("伺服就绪"), QStringLiteral("primary"));
    m_powerOnBtn = createStyledButton(QStringLiteral("上电"), QStringLiteral("success"));
    m_powerOffBtn = createStyledButton(QStringLiteral("下电"), QStringLiteral("warning"));
    m_clearErrorBtn = createStyledButton(QStringLiteral("清除报警"), QStringLiteral("danger"));

    layout->addWidget(m_servoReadyBtn, 0, 0);
    layout->addWidget(m_powerOnBtn, 0, 1);
    layout->addWidget(m_powerOffBtn, 0, 2);
    layout->addWidget(m_clearErrorBtn, 0, 3);

    m_servoStatusLabel = new QLabel(QStringLiteral("伺服状态: --"));
    layout->addWidget(m_servoStatusLabel, 1, 0, 1, 2);

    layout->addWidget(new QLabel(QStringLiteral("模式:")), 1, 2);
    m_modeCombo = new QComboBox();
    m_modeCombo->addItems({QStringLiteral("示教"), QStringLiteral("远程"), QStringLiteral("运行")});
    layout->addWidget(m_modeCombo, 1, 3);

    connect(m_servoReadyBtn, &QPushButton::clicked, this, &MainWindow::onServoReady);
    connect(m_powerOnBtn, &QPushButton::clicked, this, &MainWindow::onServoPowerOn);
    connect(m_powerOffBtn, &QPushButton::clicked, this, &MainWindow::onServoPowerOff);
    connect(m_clearErrorBtn, &QPushButton::clicked, this, &MainWindow::onClearError);
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onModeChanged);

    return group;
}

QWidget* MainWindow::createPositionPanel()
{
    QGroupBox *group = createStyledGroup(QStringLiteral("末端当前位置"));
    QGridLayout *layout = new QGridLayout(group);
    layout->setSpacing(6);

    QHBoxLayout *coordRow = new QHBoxLayout();
    coordRow->addWidget(new QLabel(QStringLiteral("控制坐标系:")));
    m_coordCombo = new QComboBox();
    m_coordCombo->addItems({QStringLiteral("关节"), QStringLiteral("直角"), QStringLiteral("工具"), QStringLiteral("用户")});
    coordRow->addWidget(m_coordCombo);

    coordRow->addSpacing(20);
    coordRow->addWidget(new QLabel(QStringLiteral("速度(%):")));
    m_speedSlider = new QSlider(Qt::Horizontal);
    m_speedSlider->setRange(1, 100);
    m_speedSlider->setValue(25);
    m_speedSlider->setMinimumWidth(110);
    coordRow->addWidget(m_speedSlider);

    m_speedSpin = new QSpinBox();
    m_speedSpin->setRange(1, 100);
    m_speedSpin->setValue(25);
    m_speedSpin->setMaximumWidth(60);
    coordRow->addWidget(m_speedSpin);

    m_speedApplyBtn = createStyledButton(QStringLiteral("设置"), QStringLiteral("primary"));
    m_speedApplyBtn->setMaximumWidth(64);
    coordRow->addWidget(m_speedApplyBtn);
    layout->addLayout(coordRow, 0, 0, 1, kVisibleAxisCount);

    QLabel *poseHintLabel = new QLabel(QStringLiteral("右侧数值固定显示末端位姿；A / B / C 为末端姿态角"));
    poseHintLabel->setProperty("styleClass", "subtle");
    layout->addWidget(poseHintLabel, 1, 0, 1, kVisibleAxisCount);

    for (int i = 0; i < kVisibleAxisCount; ++i) {
        m_posHeaderLabels[i] = new QLabel();
        m_posHeaderLabels[i]->setProperty("styleClass", "header");
        m_posHeaderLabels[i]->setAlignment(Qt::AlignCenter);
        layout->addWidget(m_posHeaderLabels[i], 2, i);

        m_posLabels[i] = new QLabel(QStringLiteral("0.000"));
        m_posLabels[i]->setProperty("styleClass", "value");
        m_posLabels[i]->setAlignment(Qt::AlignCenter);
        layout->addWidget(m_posLabels[i], 3, i);
    }

    connect(m_coordCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onCoordChanged);
    connect(m_speedSlider, &QSlider::valueChanged, this, &MainWindow::onSpeedSliderChanged);
    connect(m_speedApplyBtn, &QPushButton::clicked, this, &MainWindow::onSpeedApply);
    connect(m_speedSlider, &QSlider::valueChanged, m_speedSpin, &QSpinBox::setValue);
    connect(m_speedSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_speedSlider, &QSlider::setValue);

    return group;
}

QWidget* MainWindow::createPreviewPanel()
{
    QGroupBox *group = createStyledGroup(QStringLiteral("机械臂预览"));
    QVBoxLayout *layout = new QVBoxLayout(group);

    QHBoxLayout *topRow = new QHBoxLayout();
    m_previewPoseLabel = new QLabel(QStringLiteral("离线 | TCP X:0.0 Y:0.0 Z:0.0"));
    m_previewPoseLabel->setProperty("styleClass", "subtle");
    topRow->addWidget(m_previewPoseLabel, 1);

    QLabel *hintLabel = new QLabel(QStringLiteral("拖拽旋转 · 滚轮缩放"));
    hintLabel->setProperty("styleClass", "subtle");
    topRow->addWidget(hintLabel);

    m_clearTraceBtn = createStyledButton(QStringLiteral("清空轨迹"), QStringLiteral("warning"));
    m_clearTraceBtn->setMaximumWidth(96);
    topRow->addWidget(m_clearTraceBtn);
    layout->addLayout(topRow);

    m_robotPreview = new RobotPreviewWidget(group);
    layout->addWidget(m_robotPreview, 1);

    connect(m_clearTraceBtn, &QPushButton::clicked, this, &MainWindow::onClearPreviewTrace);
    return group;
}

QWidget* MainWindow::createJogPanel()
{
    QWidget *panel = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(panel);
    mainLayout->setSpacing(10);

    QGroupBox *jogGroup = createStyledGroup(QStringLiteral("点动操作（按下移动，松开停止）"));
    QGridLayout *jogLayout = new QGridLayout(jogGroup);
    jogLayout->setSpacing(6);

    const QStringList axisNames = {
        QStringLiteral("J1/X"), QStringLiteral("J2/Y"), QStringLiteral("J3/Z"),
        QStringLiteral("J4/A"), QStringLiteral("J5/B"), QStringLiteral("J6/C")
    };

    jogLayout->addWidget(new QLabel(QStringLiteral("轴")), 0, 0);
    QLabel *negLabel = new QLabel(QStringLiteral("负方向"));
    negLabel->setAlignment(Qt::AlignCenter);
    jogLayout->addWidget(negLabel, 0, 1);
    QLabel *posLabel = new QLabel(QStringLiteral("正方向"));
    posLabel->setAlignment(Qt::AlignCenter);
    jogLayout->addWidget(posLabel, 0, 2);

    for (int i = 0; i < axisNames.size(); ++i) {
        QLabel *axisLabel = new QLabel(axisNames[i]);
        axisLabel->setMinimumWidth(60);
        jogLayout->addWidget(axisLabel, i + 1, 0);

        m_jogNegBtn[i] = createStyledButton(QStringLiteral(" - "), QStringLiteral("jog"));
        m_jogNegBtn[i]->setProperty("axis", i);
        m_jogNegBtn[i]->setProperty("dir", false);
        jogLayout->addWidget(m_jogNegBtn[i], i + 1, 1);

        m_jogPosBtn[i] = createStyledButton(QStringLiteral(" + "), QStringLiteral("jog"));
        m_jogPosBtn[i]->setProperty("axis", i);
        m_jogPosBtn[i]->setProperty("dir", true);
        jogLayout->addWidget(m_jogPosBtn[i], i + 1, 2);

        connect(m_jogNegBtn[i], &QPushButton::pressed, this, &MainWindow::onJogStart);
        connect(m_jogNegBtn[i], &QPushButton::released, this, &MainWindow::onJogStop);
        connect(m_jogPosBtn[i], &QPushButton::pressed, this, &MainWindow::onJogStart);
        connect(m_jogPosBtn[i], &QPushButton::released, this, &MainWindow::onJogStop);
    }

    mainLayout->addWidget(jogGroup);

    QGroupBox *quickGroup = createStyledGroup(QStringLiteral("快捷动作"));
    QHBoxLayout *quickLayout = new QHBoxLayout(quickGroup);
    m_goHomeBtn = createStyledButton(QStringLiteral("回零点"), QStringLiteral("primary"));
    m_goResetBtn = createStyledButton(QStringLiteral("复位点"), QStringLiteral("primary"));
    quickLayout->addWidget(m_goHomeBtn);
    quickLayout->addWidget(m_goResetBtn);
    quickLayout->addStretch();

    connect(m_goHomeBtn, &QPushButton::clicked, this, &MainWindow::onGoHome);
    connect(m_goResetBtn, &QPushButton::clicked, this, &MainWindow::onGoReset);

    mainLayout->addWidget(quickGroup);
    mainLayout->addStretch();
    return panel;
}

QWidget* MainWindow::createMotionPanel()
{
    QWidget *panel = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(panel);

    QGroupBox *targetGroup = createStyledGroup(QStringLiteral("目标点位"));
    QGridLayout *targetLayout = new QGridLayout(targetGroup);

    const QStringList axisNames = {
        QStringLiteral("J1/X"), QStringLiteral("J2/Y"), QStringLiteral("J3/Z"), QStringLiteral("J4/A"),
        QStringLiteral("J5/B"), QStringLiteral("J6/C")
    };

    for (int i = 0; i < axisNames.size(); ++i) {
        targetLayout->addWidget(new QLabel(axisNames[i] + QStringLiteral(":")), i / 4, (i % 4) * 2);
        m_targetPos[i] = new QDoubleSpinBox();
        m_targetPos[i]->setRange(-99999.0, 99999.0);
        m_targetPos[i]->setDecimals(3);
        m_targetPos[i]->setValue(0.0);
        m_targetPos[i]->setMinimumWidth(104);
        targetLayout->addWidget(m_targetPos[i], i / 4, (i % 4) * 2 + 1);
    }
    mainLayout->addWidget(targetGroup);

    QGroupBox *paramGroup = createStyledGroup(QStringLiteral("运动参数"));
    QHBoxLayout *paramLayout = new QHBoxLayout(paramGroup);
    paramLayout->addWidget(new QLabel(QStringLiteral("速度:")));
    m_moveVelSpin = new QSpinBox();
    m_moveVelSpin->setRange(1, 1000);
    m_moveVelSpin->setValue(80);
    paramLayout->addWidget(m_moveVelSpin);

    paramLayout->addWidget(new QLabel(QStringLiteral("加速度:")));
    m_moveAccSpin = new QSpinBox();
    m_moveAccSpin->setRange(1, 100);
    m_moveAccSpin->setValue(50);
    paramLayout->addWidget(m_moveAccSpin);

    paramLayout->addWidget(new QLabel(QStringLiteral("减速度:")));
    m_moveDecSpin = new QSpinBox();
    m_moveDecSpin->setRange(1, 100);
    m_moveDecSpin->setValue(50);
    paramLayout->addWidget(m_moveDecSpin);
    paramLayout->addStretch();
    mainLayout->addWidget(paramGroup);

    QGroupBox *cmdGroup = createStyledGroup(QStringLiteral("执行运动"));
    QHBoxLayout *cmdLayout = new QHBoxLayout(cmdGroup);
    m_moveJBtn = createStyledButton(QStringLiteral("MOVJ（关节运动）"), QStringLiteral("primary"));
    m_moveLBtn = createStyledButton(QStringLiteral("MOVL（直线运动）"), QStringLiteral("primary"));
    cmdLayout->addWidget(m_moveJBtn);
    cmdLayout->addWidget(m_moveLBtn);
    cmdLayout->addStretch();

    connect(m_moveJBtn, &QPushButton::clicked, this, &MainWindow::onMoveJ);
    connect(m_moveLBtn, &QPushButton::clicked, this, &MainWindow::onMoveL);

    mainLayout->addWidget(cmdGroup);
    mainLayout->addStretch();
    return panel;
}

QWidget* MainWindow::createJobPanel()
{
    QWidget *panel = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(panel);

    QGroupBox *fileGroup = createStyledGroup(QStringLiteral("作业文件"));
    QGridLayout *fileLayout = new QGridLayout(fileGroup);
    fileLayout->addWidget(new QLabel(QStringLiteral("文件名:")), 0, 0);
    m_jobNameEdit = new QLineEdit(QStringLiteral("TEST"));
    fileLayout->addWidget(m_jobNameEdit, 0, 1, 1, 2);

    m_jobCreateBtn = createStyledButton(QStringLiteral("新建"), QStringLiteral("success"));
    m_jobOpenBtn = createStyledButton(QStringLiteral("打开"), QStringLiteral("primary"));
    m_jobDeleteBtn = createStyledButton(QStringLiteral("删除"), QStringLiteral("danger"));
    fileLayout->addWidget(m_jobCreateBtn, 1, 0);
    fileLayout->addWidget(m_jobOpenBtn, 1, 1);
    fileLayout->addWidget(m_jobDeleteBtn, 1, 2);

    connect(m_jobCreateBtn, &QPushButton::clicked, this, &MainWindow::onJobCreate);
    connect(m_jobOpenBtn, &QPushButton::clicked, this, &MainWindow::onJobOpen);
    connect(m_jobDeleteBtn, &QPushButton::clicked, this, &MainWindow::onJobDelete);

    mainLayout->addWidget(fileGroup);

    QGroupBox *runGroup = createStyledGroup(QStringLiteral("运行控制"));
    QGridLayout *runLayout = new QGridLayout(runGroup);
    m_jobRunBtn = createStyledButton(QStringLiteral("运行"), QStringLiteral("success"));
    m_jobPauseBtn = createStyledButton(QStringLiteral("暂停"), QStringLiteral("warning"));
    m_jobContinueBtn = createStyledButton(QStringLiteral("继续"), QStringLiteral("primary"));
    m_jobStopBtn = createStyledButton(QStringLiteral("停止"), QStringLiteral("danger"));

    runLayout->addWidget(m_jobRunBtn, 0, 0);
    runLayout->addWidget(m_jobPauseBtn, 0, 1);
    runLayout->addWidget(m_jobContinueBtn, 0, 2);
    runLayout->addWidget(m_jobStopBtn, 0, 3);

    runLayout->addWidget(new QLabel(QStringLiteral("运行次数:")), 1, 0);
    m_jobRunTimesSpin = new QSpinBox();
    m_jobRunTimesSpin->setRange(0, 9999);
    m_jobRunTimesSpin->setValue(1);
    m_jobRunTimesSpin->setSpecialValueText(QStringLiteral("无限"));
    runLayout->addWidget(m_jobRunTimesSpin, 1, 1);

    m_jobCurrentLabel = new QLabel(QStringLiteral("当前文件: --"));
    m_jobLineLabel = new QLabel(QStringLiteral("当前行号: --"));
    runLayout->addWidget(m_jobCurrentLabel, 2, 0, 1, 2);
    runLayout->addWidget(m_jobLineLabel, 2, 2, 1, 2);

    connect(m_jobRunBtn, &QPushButton::clicked, this, &MainWindow::onJobRun);
    connect(m_jobPauseBtn, &QPushButton::clicked, this, &MainWindow::onJobPause);
    connect(m_jobContinueBtn, &QPushButton::clicked, this, &MainWindow::onJobContinue);
    connect(m_jobStopBtn, &QPushButton::clicked, this, &MainWindow::onJobStop);

    mainLayout->addWidget(runGroup);
    mainLayout->addStretch();
    return panel;
}

QWidget* MainWindow::createIOPanel()
{
    QWidget *panel = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(panel);

    QGroupBox *doSetGroup = createStyledGroup(QStringLiteral("数字输出设置"));
    QHBoxLayout *doSetLayout = new QHBoxLayout(doSetGroup);
    doSetLayout->addWidget(new QLabel(QStringLiteral("端口:")));
    m_doPortSpin = new QSpinBox();
    m_doPortSpin->setRange(1, 64);
    doSetLayout->addWidget(m_doPortSpin);

    doSetLayout->addWidget(new QLabel(QStringLiteral("值:")));
    m_doValueCombo = new QComboBox();
    m_doValueCombo->addItems({QStringLiteral("OFF (0)"), QStringLiteral("ON (1)")});
    doSetLayout->addWidget(m_doValueCombo);

    m_doSetBtn = createStyledButton(QStringLiteral("设置输出"), QStringLiteral("primary"));
    m_ioRefreshBtn = createStyledButton(QStringLiteral("刷新IO"), QStringLiteral("warning"));
    doSetLayout->addWidget(m_doSetBtn);
    doSetLayout->addWidget(m_ioRefreshBtn);
    doSetLayout->addStretch();

    connect(m_doSetBtn, &QPushButton::clicked, this, &MainWindow::onSetDigitalOutput);
    connect(m_ioRefreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshIO);

    mainLayout->addWidget(doSetGroup);

    QHBoxLayout *ioTableLayout = new QHBoxLayout();

    QGroupBox *diGroup = createStyledGroup(QStringLiteral("数字输入 (DI)"));
    QVBoxLayout *diLayout = new QVBoxLayout(diGroup);
    m_diTable = new QTableWidget(8, 4);
    m_diTable->setHorizontalHeaderLabels({QStringLiteral("端口"), QStringLiteral("状态"), QStringLiteral("端口"), QStringLiteral("状态")});
    m_diTable->horizontalHeader()->setStretchLastSection(true);
    m_diTable->verticalHeader()->setVisible(false);
    m_diTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_diTable->setMaximumHeight(220);
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 2; ++c) {
            const int port = r + c * 8 + 1;
            m_diTable->setItem(r, c * 2, new QTableWidgetItem(QStringLiteral("DI%1").arg(port)));
            m_diTable->setItem(r, c * 2 + 1, new QTableWidgetItem(QStringLiteral("--")));
        }
    }
    diLayout->addWidget(m_diTable);
    ioTableLayout->addWidget(diGroup);

    QGroupBox *doGroup = createStyledGroup(QStringLiteral("数字输出 (DO)"));
    QVBoxLayout *doLayout = new QVBoxLayout(doGroup);
    m_doTable = new QTableWidget(8, 4);
    m_doTable->setHorizontalHeaderLabels({QStringLiteral("端口"), QStringLiteral("状态"), QStringLiteral("端口"), QStringLiteral("状态")});
    m_doTable->horizontalHeader()->setStretchLastSection(true);
    m_doTable->verticalHeader()->setVisible(false);
    m_doTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_doTable->setMaximumHeight(220);
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 2; ++c) {
            const int port = r + c * 8 + 1;
            m_doTable->setItem(r, c * 2, new QTableWidgetItem(QStringLiteral("DO%1").arg(port)));
            m_doTable->setItem(r, c * 2 + 1, new QTableWidgetItem(QStringLiteral("--")));
        }
    }
    doLayout->addWidget(m_doTable);
    ioTableLayout->addWidget(doGroup);

    mainLayout->addLayout(ioTableLayout);
    mainLayout->addStretch();
    return panel;
}

QWidget* MainWindow::createGlobalVariantPanel()
{
    QWidget *panel = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(panel);

    m_globalVarModeLabel = new QLabel(QStringLiteral("当前模式: 离线（未连接控制器）"));
    m_globalVarModeLabel->setProperty("styleClass", "status-disconnected");
    mainLayout->addWidget(m_globalVarModeLabel);

    QGroupBox *editGroup = createStyledGroup(QStringLiteral("全局变量读写"));
    QGridLayout *editLayout = new QGridLayout(editGroup);
    editLayout->setSpacing(8);

    editLayout->addWidget(new QLabel(QStringLiteral("变量名:")), 0, 0);
    m_globalVarNameEdit = new QLineEdit();
    m_globalVarNameEdit->setPlaceholderText(QStringLiteral("例如 GI001 / GD001 / GB001"));
    m_globalVarNameEdit->setMinimumWidth(140);
    editLayout->addWidget(m_globalVarNameEdit, 0, 1);

    editLayout->addWidget(new QLabel(QStringLiteral("变量值:")), 0, 2);
    m_globalVarValueSpin = new QDoubleSpinBox();
    m_globalVarValueSpin->setRange(-1.0e12, 1.0e12);
    m_globalVarValueSpin->setDecimals(6);
    m_globalVarValueSpin->setValue(0.0);
    m_globalVarValueSpin->setMinimumWidth(140);
    editLayout->addWidget(m_globalVarValueSpin, 0, 3);

    m_globalVarTypeLabel = new QLabel(QStringLiteral("类型: --"));
    m_globalVarTypeLabel->setProperty("styleClass", "subtle");
    m_globalVarTypeLabel->setMinimumWidth(140);
    editLayout->addWidget(m_globalVarTypeLabel, 0, 4);

    m_globalVarSetBtn = createStyledButton(QStringLiteral("设置变量"), QStringLiteral("primary"));
    m_globalVarGetBtn = createStyledButton(QStringLiteral("获取变量"), QStringLiteral("success"));
    editLayout->addWidget(m_globalVarSetBtn, 0, 5);
    editLayout->addWidget(m_globalVarGetBtn, 0, 6);

    editLayout->addWidget(new QLabel(QStringLiteral("常用点位:")), 1, 0);
    m_globalVarPresetCombo = new QComboBox();
    m_globalVarPresetCombo->setMinimumWidth(360);

    struct PresetEntry { const char* name; const char* desc; };
    static const PresetEntry kPresets[] = {
        {"GB001", "请求补刀（1=请求补刀/耗材库寿命）"},
        {"GB002", "当日产量清零"},
        {"GB003", "补刀完成"},
        {"GB004", "刀库开关门（1=开 / 0=关）"},
        {"GB009", "打磨开始信号"},
        {"GB019", "B0 力控正反反馈（0=正向 1=反向）"},
        {"GB024", "主轴 B1 夹刀到位"},
        {"GB025", "主轴 B1 松刀到位"},
        {"GB027", "主轴 B1 零速"},
        {"GB028", "主轴 B1 达速"},
        {"GB033", "主轴 B2 关到位"},
        {"GB034", "主轴 B2 松到位"},
        {"GB036", "主轴 B2 零速"},
        {"GB037", "主轴 B2 达速"},
        {"GB038", "B0 正/反向切换"},
        {"GB039", "主轴 B1 启动（上升沿）"},
        {"GB040", "主轴 B1 停止（上升沿）"},
        {"GB041", "主轴 B1 松夹刀阀（1=松 0=夹）"},
        {"GB043", "主轴 B1 气动工具"},
        {"GB044", "主轴 B2 启动（上升沿）"},
        {"GB045", "主轴 B2 停止（上升沿）"},
        {"GB046", "主轴 B2 松夹刀阀（1=松 0=夹）"},
        {"GB048", "主轴 B2 气动工具"},
        {"GB053", "A1 力控正反反馈（0=正 1=反）"},
        {"GB058", "主轴 A1 夹刀到位"},
        {"GB059", "主轴 A1 松刀到位"},
        {"GB061", "主轴 A1 零速"},
        {"GB062", "主轴 A1 达速"},
        {"GB064", "主轴 A1 未张紧（1=未张紧）"},
        {"GB070", "主轴 A2 关到位"},
        {"GB071", "主轴 A2 松到位"},
        {"GB073", "主轴 A2 零速"},
        {"GB074", "主轴 A2 达速"},
        {"GB075", "A0 正/反向切换"},
        {"GB076", "主轴 A1 启动（上升沿）"},
        {"GB077", "主轴 A1 停止（上升沿）"},
        {"GB078", "主轴 A1 松夹刀阀（1=松 0=夹）"},
        {"GB080", "主轴 A1 气动工具"},
        {"GB081", "主轴 A2 启动（上升沿）"},
        {"GB082", "主轴 A2 停止（上升沿）"},
        {"GB083", "主轴 A2 松夹刀阀（1=松 0=夹）"},
        {"GB085", "主轴 A2 气动工具"},
        {"GB092", "夹爪 1 输出映射 1"},
        {"GB093", "夹爪 2 输出映射 1"},
        {"GB094", "料盘 1 输出映射 1"},
        {"GB095", "料盘 2 输出映射 1"},
        {"GB096", "预留输出映射 1"},
        {"GB097", "预留输出映射 2"},
        {"GB098", "刀库开关门 开到位"},
        {"GB099", "刀库开关门 关到位"},
        {"GB101", "全 1 暂停检测组 1"},
        {"GB102", "全 1 暂停检测组 2"},
        {"GB103", "全 1 暂停检测组 3"},
        {"GB104", "全 1 暂停检测组 4"},
        {"GB105", "全 1 暂停检测组 5"},
        {"GB106", "全 1 暂停检测组 6"},
        {"GB107", "全 1 暂停检测组 7（光栅）"},
        {"GB108", "全 1 暂停检测组 8（夹具 A）"},
        {"GB109", "全 1 暂停检测组 9（夹具 B）"},
        {"GB110", "全 1 暂停检测组 10（安全门）"},
        {"GB111", "全 0 暂停检测组 1"},
        {"GB112", "全 0 暂停检测组 2"},
        {"GB113", "全 0 暂停检测组 3"},
        {"GB114", "全 0 暂停检测组 4"},
        {"GB115", "全 0 暂停检测组 5"},
        {"GB116", "全 0 暂停检测组 6"},
        {"GB117", "全 0 暂停检测组 7"},
        {"GB118", "全 0 暂停检测组 8"},
        {"GB119", "全 0 暂停检测组 9"},
        {"GB120", "全 0 暂停检测组 10"},
        {"GB121", "定时器 1（超时报警）"},
        {"GB122", "定时器 2（超时报警）"},
        {"GB123", "定时器 3（超时报警）"},
        {"GB124", "定时器 4（超时报警）"},
        {"GB125", "定时器 5（超时报警）"},
        {"GB126", "定时器 6（超时报警）"},
        {"GB131", "单一耗材功能启用 01"},
        {"GB132", "单一耗材功能启用 02"},
        {"GB133", "单一耗材功能启用 03"},
        {"GB134", "单一耗材功能启用 04"},
        {"GB135", "单一耗材功能启用 05"},
        {"GB136", "单一耗材功能启用 06"},
        {"GB137", "单一耗材功能启用 07"},
        {"GB138", "单一耗材功能启用 08"},
        {"GB139", "单一耗材功能启用 09"},
        {"GB140", "单一耗材功能启用 10"},
        {"GB141", "单一耗材功能启用 11"},
        {"GB142", "单一耗材功能启用 12"},
        {"GB143", "单一耗材功能启用 13"},
        {"GB144", "单一耗材功能启用 14"},
        {"GB145", "单一耗材功能启用 15"},
        {"GB146", "单一耗材功能启用 16"},
        {"GB151", "砂纸库输入映射（1=有 0=无）"},
        {"GB152", "砂纸库顶出气缸 1"},
        {"GB153", "砂纸库顶出气缸 2"},
        {"GB154", "砂纸库顶出气缸 3"},
        {"GB155", "砂纸库顶出气缸 4"},
        {"GB156", "砂纸库顶出气缸 5"},
        {"GB157", "砂纸库顶出气缸 6"},
        {"GB158", "砂纸库顶出气缸 7"},
        {"GB159", "砂纸库顶出气缸 8"},
        {"GB160", "下班"},
        {"GB161", "夹爪 1 输入映射 1"},
        {"GB162", "夹爪 1 输入映射 2"},
        {"GB163", "夹爪 1 输入映射 3"},
        {"GB164", "夹爪 2 输入映射 1"},
        {"GB165", "夹爪 2 输入映射 2"},
        {"GB166", "夹爪 2 输入映射 3"},
        {"GB167", "LP1 输入映射 1"},
        {"GB168", "LP1 输入映射 2"},
        {"GB169", "LP2 输入映射 1"},
        {"GB170", "LP2 输入映射 2"},
        {"GB172", "预约组 1 输出位映射（A 就绪灯）"},
        {"GB174", "预约组 2 输出位映射（B 就绪灯）"},
        {"GB175", "初始化"},
        {"GD096", "变位机 B 工位上限"},
        {"GD097", "变位机 B 工位下限"},
        {"GD098", "变位机 A 工位上限"},
        {"GD099", "变位机 A 工位下限"},
        {"GD100", "变位机当前角度（7 轴）"},
        {"GI029", "B0 行程反馈值（0.1mm）"},
        {"GI030", "B0 当前力反馈值（0.1N）"},
        {"GI031", "B1 反馈转速（rpm）"},
        {"GI036", "B2 反馈转速（rpm）"},
        {"GI045", "B0 打磨力设定值（0.1N）"},
        {"GI046", "B0 工具重量（0.1kg）"},
        {"GI047", "B0 报警行程（0.1mm）"},
        {"GI048", "B0 接触力设定（0.1N）"},
        {"GI049", "B0 过渡时间设定（0.1s）"},
        {"GI050", "B1 转速设定（rpm）"},
        {"GI051", "B2 转速设定（rpm）"},
        {"GI052", "A0 行程反馈值（0.1mm）"},
        {"GI053", "A0 当前力反馈值（0.1N）"},
        {"GI054", "主轴 A1 反馈转速（rpm）"},
        {"GI059", "主轴 A2 反馈转速（rpm）"},
        {"GI068", "A0 打磨力设定值（0.1N）"},
        {"GI069", "A0 工具重量（0.1kg）"},
        {"GI070", "A0 报警行程（0.1mm）"},
        {"GI071", "A0 接触力设定（0.1N）"},
        {"GI072", "A0 过渡时间设定（0.1s）"},
        {"GI073", "主轴 A1 转速设定（rpm）"},
        {"GI074", "主轴 A2 转速设定（rpm）"},
        {"GI102", "耗材 01 取刀计数"},
        {"GI103", "耗材 02 取刀计数"},
        {"GI104", "耗材 03 取刀计数"},
        {"GI105", "耗材 04 取刀计数"},
        {"GI106", "耗材 05 取刀计数"},
        {"GI107", "耗材 06 取刀计数"},
        {"GI108", "耗材 07 取刀计数"},
        {"GI109", "耗材 08 取刀计数"},
        {"GI110", "耗材 09 取刀计数"},
        {"GI111", "耗材 10 取刀计数"},
        {"GI112", "耗材 11 取刀计数"},
        {"GI113", "耗材 12 取刀计数"},
        {"GI114", "耗材 13 取刀计数"},
        {"GI115", "耗材 14 取刀计数"},
        {"GI116", "耗材 15 取刀计数"},
        {"GI117", "耗材 16 取刀计数"},
        {"GI131", "耗材 1 当前组号（=10 寿命到达）"},
        {"GI132", "耗材 2 当前组号（=10 寿命到达）"},
        {"GI133", "耗材 3 当前组号（=10 寿命到达）"},
        {"GI134", "耗材 4 当前组号（=10 寿命到达）"},
        {"GI135", "耗材 5 当前组号（=10 寿命到达）"},
        {"GI136", "耗材 6 当前组号（=10 寿命到达）"},
        {"GI137", "耗材 7 当前组号（=10 寿命到达）"},
        {"GI138", "耗材 8 当前组号（=10 寿命到达）"},
        {"GI139", "耗材 9 当前组号（=10 寿命到达）"},
        {"GI140", "耗材 10 当前组号（=10 寿命到达）"},
        {"GI141", "耗材 11 当前组号（=10 寿命到达）"},
        {"GI142", "耗材 12 当前组号（=10 寿命到达）"},
        {"GI143", "耗材 13 当前组号（=10 寿命到达）"},
        {"GI144", "耗材 14 当前组号（=10 寿命到达）"},
        {"GI145", "耗材 15 当前组号（=10 寿命到达）"},
        {"GI146", "耗材 16 当前组号（=10 寿命到达）"},
    };

    m_globalVarPresetCombo->addItem(QStringLiteral("-- 选择常用变量自动填入名称 --"), QString());
    for (const PresetEntry& entry : kPresets) {
        const QString name = QString::fromLatin1(entry.name);
        const QString desc = QString::fromUtf8(entry.desc);
        m_globalVarPresetCombo->addItem(QStringLiteral("%1 — %2").arg(name, desc), name);
    }
    editLayout->addWidget(m_globalVarPresetCombo, 1, 1, 1, 6);

    m_globalVarResultLabel = new QLabel(QStringLiteral("最近读取: --"));
    m_globalVarResultLabel->setProperty("styleClass", "value");
    editLayout->addWidget(m_globalVarResultLabel, 2, 0, 1, 7);

    QLabel *hintLabel = new QLabel(QStringLiteral(
        "命名规则：GI 表示全局整型，GD 表示全局双精度浮点，GB 表示全局布尔；"
        "如 GI001、GD012、GB003。可在“常用点位”中选择以快速填入名称。"));
    hintLabel->setProperty("styleClass", "subtle");
    hintLabel->setWordWrap(true);
    editLayout->addWidget(hintLabel, 3, 0, 1, 7);

    connect(m_globalVarSetBtn, &QPushButton::clicked, this, &MainWindow::onSetGlobalVariant);
    connect(m_globalVarGetBtn, &QPushButton::clicked, this, &MainWindow::onGetGlobalVariant);
    connect(m_globalVarPresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                const QString name = m_globalVarPresetCombo->itemData(index).toString();
                if (!name.isEmpty()) {
                    m_globalVarNameEdit->setText(name);
                }
            });
    connect(m_globalVarNameEdit, &QLineEdit::textChanged,
            this, [this](const QString&) { applyGlobalVariantInputType(); });
    applyGlobalVariantInputType();
    refreshGlobalVariantModeLabel();

    mainLayout->addWidget(editGroup);

    QGroupBox *actionGroup = createStyledGroup(QStringLiteral("快捷操作"));
    QHBoxLayout *actionLayout = new QHBoxLayout(actionGroup);
    m_windGrinderBtn = createStyledButton(QStringLiteral("启动风磨"), QStringLiteral("success"));
    m_windGrinderBtn->setMinimumWidth(140);
    actionLayout->addWidget(m_windGrinderBtn);
    actionLayout->addStretch();
    connect(m_windGrinderBtn, &QPushButton::clicked, this, &MainWindow::onToggleWindGrinder);
    mainLayout->addWidget(actionGroup);

    mainLayout->addStretch();
    return panel;
}

void MainWindow::applyGlobalVariantInputType()
{
    if (!m_globalVarValueSpin || !m_globalVarTypeLabel) {
        return;
    }

    const QString name = m_globalVarNameEdit ? m_globalVarNameEdit->text().trimmed().toUpper()
                                              : QString();

    if (name.startsWith(QStringLiteral("GB"))) {
        m_globalVarValueSpin->setDecimals(0);
        m_globalVarValueSpin->setRange(0.0, 1.0);
        m_globalVarValueSpin->setSingleStep(1.0);
        if (m_globalVarValueSpin->value() != 0.0 && m_globalVarValueSpin->value() != 1.0) {
            m_globalVarValueSpin->setValue(m_globalVarValueSpin->value() >= 0.5 ? 1.0 : 0.0);
        }
        m_globalVarTypeLabel->setText(QStringLiteral("类型: 布尔 (0=否, 1=是)"));
    } else if (name.startsWith(QStringLiteral("GI"))) {
        m_globalVarValueSpin->setDecimals(0);
        m_globalVarValueSpin->setRange(-2147483648.0, 2147483647.0);
        m_globalVarValueSpin->setSingleStep(1.0);
        m_globalVarTypeLabel->setText(QStringLiteral("类型: 整型 (32 位整数)"));
    } else if (name.startsWith(QStringLiteral("GD"))) {
        m_globalVarValueSpin->setDecimals(6);
        m_globalVarValueSpin->setRange(-1.0e12, 1.0e12);
        m_globalVarValueSpin->setSingleStep(0.01);
        m_globalVarTypeLabel->setText(QStringLiteral("类型: 双精度浮点"));
    } else {
        m_globalVarValueSpin->setDecimals(6);
        m_globalVarValueSpin->setRange(-1.0e12, 1.0e12);
        m_globalVarValueSpin->setSingleStep(0.01);
        m_globalVarTypeLabel->setText(QStringLiteral("类型: --"));
    }
}

void MainWindow::refreshGlobalVariantModeLabel()
{
    if (!m_globalVarModeLabel) {
        return;
    }

    if (!m_connected) {
        m_globalVarModeLabel->setText(QStringLiteral("当前模式: 离线（未连接控制器，点击 [连接] 后再使用）"));
        m_globalVarModeLabel->setProperty("styleClass", "status-disconnected");
    } else if (m_demoMode || m_socketFd == -1) {
        m_globalVarModeLabel->setText(QStringLiteral("当前模式: 演示模式（未连到真机，SDK 不会下发，仅本地回显）"));
        m_globalVarModeLabel->setProperty("styleClass", "status-disconnected");
    } else {
        m_globalVarModeLabel->setText(QStringLiteral("当前模式: 真机（socket=%1，已连接控制器）").arg(m_socketFd));
        m_globalVarModeLabel->setProperty("styleClass", "status-connected");
    }
    m_globalVarModeLabel->style()->unpolish(m_globalVarModeLabel);
    m_globalVarModeLabel->style()->polish(m_globalVarModeLabel);
}


QPushButton* MainWindow::createStyledButton(const QString& text, const QString& styleClass)
{
    QPushButton *button = new QPushButton(text);
    if (!styleClass.isEmpty()) {
        button->setProperty("styleClass", styleClass);
    }
    return button;
}

QGroupBox* MainWindow::createStyledGroup(const QString& title)
{
    return new QGroupBox(title);
}

void MainWindow::appendLog(const QString& msg)
{
    if (!m_logEdit) {
        return;
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    m_logEdit->append(QStringLiteral("<span style='color:#585b70;'>%1</span> %2").arg(timestamp, msg));

    QTextCursor cursor = m_logEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logEdit->setTextCursor(cursor);
}

void MainWindow::stopTimers()
{
    if (m_updateTimer && m_updateTimer->isActive()) {
        m_updateTimer->stop();
    }
    if (m_previewTimer && m_previewTimer->isActive()) {
        m_previewTimer->stop();
    }
}

void MainWindow::setConnectionUi(bool connected)
{
    m_connectBtn->setEnabled(!connected);
    m_disconnectBtn->setEnabled(connected);
    m_ipEdit->setEnabled(!connected);
    m_portEdit->setEnabled(!connected);
}

void MainWindow::updateConnectionStatusLabel(const QString& text, bool connectedStyle)
{
    m_connStatusLabel->setText(text);
    m_connStatusLabel->setProperty("styleClass", connectedStyle ? "status-connected" : "status-disconnected");
    m_connStatusLabel->style()->unpolish(m_connStatusLabel);
    m_connStatusLabel->style()->polish(m_connStatusLabel);
}

void MainWindow::refreshPositionHeaders()
{
    const QStringList cartesianHeaders = {
        QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z"), QStringLiteral("A"),
        QStringLiteral("B"), QStringLiteral("C")
    };

    for (int i = 0; i < kVisibleAxisCount; ++i) {
        m_posHeaderLabels[i]->setText(cartesianHeaders[i]);
    }
}

void MainWindow::refreshPositionDisplay()
{
    const std::array<double, 7>& pose = m_cartesianPose;
    for (int i = 0; i < kVisibleAxisCount; ++i) {
        m_posLabels[i]->setText(QString::number(pose[i], 'f', 3));
    }
}

void MainWindow::refreshPreview(bool appendTrace)
{
    const QVector3D tcp = poseToVector3D(m_cartesianPose);
    m_robotPreview->setRobotState(m_jointPose, tcp, appendTrace);

    const QString source = !m_connected ? QStringLiteral("离线")
        : (m_demoMode ? QStringLiteral("演示模式") : QStringLiteral("真机数据"));
    m_previewPoseLabel->setText(QStringLiteral("%1 | TCP X:%2  Y:%3  Z:%4")
                                    .arg(source)
                                    .arg(tcp.x(), 0, 'f', 1)
                                    .arg(tcp.y(), 0, 'f', 1)
                                    .arg(tcp.z(), 0, 'f', 1));
}

void MainWindow::refreshJobStatus()
{
    if (!m_connected || m_demoMode || m_socketFd == -1) {
        m_jobCurrentLabel->setText(QStringLiteral("当前文件: --"));
        m_jobLineLabel->setText(QStringLiteral("当前行号: --"));
        return;
    }

    std::string jobName;
    const Result fileResult = job_get_current_file(m_socketFd, jobName);
    if (fileResult == SUCCESS && !jobName.empty()) {
        m_jobCurrentLabel->setText(QStringLiteral("当前文件: %1").arg(QString::fromStdString(jobName)));
    } else {
        m_jobCurrentLabel->setText(QStringLiteral("当前文件: --"));
    }

    int line = 0;
    const Result lineResult = job_get_current_line(m_socketFd, line);
    if (lineResult == SUCCESS && line > 0) {
        m_jobLineLabel->setText(QStringLiteral("当前行号: %1").arg(line));
    } else {
        m_jobLineLabel->setText(QStringLiteral("当前行号: --"));
    }
}

void MainWindow::refreshStatusSummary()
{
    if (!m_connected) {
        m_servoStatusLabel->setText(QStringLiteral("伺服状态: --"));
        m_robotRunStateLabel->setText(QStringLiteral("运行状态: --"));
        m_robotTypeLabel->setText(QStringLiteral("机器人类型: --"));
        return;
    }

    m_servoStatusLabel->setText(QStringLiteral("伺服状态: %1").arg(servoStateText(m_servoState)));
    m_robotRunStateLabel->setText(QStringLiteral("运行状态: %1").arg(runStateText(m_runState)));
    m_robotTypeLabel->setText(QStringLiteral("机器人类型: %1").arg(robotTypeName(m_robotType)));
}

void MainWindow::resetDemoState()
{
    m_jointPose = {0.0, 42.0, 58.0, -18.0, 10.0, -22.0, 0.0};
    m_jointTargetPose = m_jointPose;
    m_previewMotionMode = PreviewMotionMode::None;
    m_motionVelocity = 30;
    m_activeJogAxis = -1;
    m_activeJogDirection = 0;
    m_servoState = 1;
    m_runState = 0;
    m_robotType = 12;
    updateCartesianFromJointState();
    m_cartesianTargetPose = m_cartesianPose;
}

void MainWindow::updateCartesianFromJointState()
{
    const QVector3D tcp = RobotPreviewWidget::toolPositionFromJoints(m_jointPose);
    m_cartesianPose[0] = tcp.x();
    m_cartesianPose[1] = tcp.y();
    m_cartesianPose[2] = tcp.z();
    m_cartesianPose[3] = m_jointPose[4];
    m_cartesianPose[4] = m_jointPose[3];
    m_cartesianPose[5] = m_jointPose[5];
    m_cartesianPose[6] = 0.0;
}

void MainWindow::updateJointFromCartesianState()
{
    m_jointPose = RobotPreviewWidget::approximateJointsFromTcp(m_cartesianPose);
    clearHiddenAxis(m_jointPose);
}

void MainWindow::startJointAnimation(const std::array<double, 7>& joints, int velocityPercent)
{
    m_jointTargetPose = joints;
    clearHiddenAxis(m_jointTargetPose);
    m_previewMotionMode = PreviewMotionMode::JointTarget;
    m_motionVelocity = velocityPercent;
    m_runState = 2;
}

void MainWindow::startLinearAnimation(const std::array<double, 7>& cartesianPose, int velocity)
{
    m_cartesianTargetPose = cartesianPose;
    clearHiddenAxis(m_cartesianTargetPose);
    m_previewMotionMode = PreviewMotionMode::LinearTarget;
    m_motionVelocity = velocity;
    m_runState = 2;
}

void MainWindow::stepDemoMotion()
{
    if (m_activeJogAxis >= 0) {
        const double step = qMax(0.2, m_speedSpin->value() * 0.018);
        m_jointPose[m_activeJogAxis] += step * m_activeJogDirection;
        updateCartesianFromJointState();
        m_runState = 2;
        return;
    }

    if (m_previewMotionMode == PreviewMotionMode::None) {
        m_runState = 0;
        return;
    }

    if (m_previewMotionMode == PreviewMotionMode::JointTarget) {
        const double step = qMax(0.2, m_motionVelocity * 0.045);
        bool finished = true;
        for (int i = 0; i < kInternalAxisCount; ++i) {
            const double next = stepTowards(m_jointPose[i], m_jointTargetPose[i], step);
            finished = finished && qFuzzyCompare(next + 1.0, m_jointTargetPose[i] + 1.0);
            m_jointPose[i] = next;
        }
        updateCartesianFromJointState();
        if (finished) {
            m_jointPose = m_jointTargetPose;
            updateCartesianFromJointState();
            m_previewMotionMode = PreviewMotionMode::None;
            m_runState = 0;
        }
        return;
    }

    const double linearStep = qMax(1.0, m_motionVelocity * 0.16);
    bool finished = true;
    for (int i = 0; i < kInternalAxisCount; ++i) {
        const double step = (i < 3) ? linearStep : qMax(0.2, linearStep * 0.15);
        const double next = stepTowards(m_cartesianPose[i], m_cartesianTargetPose[i], step);
        finished = finished && qFuzzyCompare(next + 1.0, m_cartesianTargetPose[i] + 1.0);
        m_cartesianPose[i] = next;
    }
    updateJointFromCartesianState();
    if (finished) {
        m_cartesianPose = m_cartesianTargetPose;
        updateJointFromCartesianState();
        m_previewMotionMode = PreviewMotionMode::None;
        m_runState = 0;
    }
}

bool MainWindow::ensureJobControllerReady(const QString& actionName)
{
    if (!m_connected) {
        appendLog(QStringLiteral("<span style='color:#f9e2af;'>%1失败：请先连接控制器。</span>").arg(actionName));
        return false;
    }

    if (m_demoMode || m_socketFd == -1) {
        appendLog(QStringLiteral("<span style='color:#f9e2af;'>%1失败：演示模式不支持作业文件操作，请连接真实控制器。</span>").arg(actionName));
        return false;
    }

    return true;
}

bool MainWindow::pollRobotState()
{
    if (!m_connected || m_demoMode || m_socketFd == -1) {
        return false;
    }

    bool updated = false;

    std::vector<double> jointPose;
    if (get_current_position(m_socketFd, 0, jointPose) == SUCCESS && jointPose.size() >= 6) {
        m_jointPose.fill(0.0);
        for (int i = 0; i < kVisibleAxisCount; ++i) {
            m_jointPose[i] = jointPose[static_cast<size_t>(i)];
        }
        updated = true;
    }

    std::vector<double> cartPose;
    if (get_current_position(m_socketFd, 1, cartPose) == SUCCESS && cartPose.size() >= 6) {
        m_cartesianPose.fill(0.0);
        for (int i = 0; i < kVisibleAxisCount; ++i) {
            m_cartesianPose[i] = cartPose[static_cast<size_t>(i)];
        }
        updated = true;
    } else if (updated) {
        updateCartesianFromJointState();
    }

    int servoState = 0;
    if (get_servo_state(m_socketFd, servoState) == SUCCESS) {
        m_servoState = servoState;
    }

    int runState = 0;
    if (get_robot_running_state(m_socketFd, runState) == SUCCESS) {
        m_runState = runState;
    }

    int robotType = 0;
    if (get_robot_type(m_socketFd, robotType) == SUCCESS) {
        m_robotType = robotType;
    }

    return updated;
}

MoveCmd MainWindow::buildMoveCommand() const
{
    MoveCmd cmd{};
    cmd.targetPosType = PosType::data;
    cmd.coord = m_coordCombo->currentIndex();
    cmd.velocity = m_moveVelSpin->value();
    cmd.acc = m_moveAccSpin->value();
    cmd.dec = m_moveDecSpin->value();
    for (int i = 0; i < kVisibleAxisCount; ++i) {
        cmd.targetPosValue[static_cast<size_t>(i)] = m_targetPos[i]->value();
    }
    cmd.targetPosValue[6] = 0.0;
    return cmd;
}

QString MainWindow::normalizedJobName() const
{
    QString jobName = m_jobNameEdit->text().trimmed();
    if (jobName.endsWith(QStringLiteral(".JBR"), Qt::CaseInsensitive)) {
        jobName.chop(4);
    }
    return jobName;
}

QString MainWindow::resultToString(Result result) const
{
    switch (result) {
    case SUCCESS:
        return QStringLiteral("SUCCESS");
    case RECEIVE_FAILED:
        return QStringLiteral("RECEIVE_FAILED");
    case DISCONNECT:
        return QStringLiteral("DISCONNECT");
    case PARAM_ERR:
        return QStringLiteral("PARAM_ERR");
    case OPERATION_NOT_ALLOWED:
        return QStringLiteral("OPERATION_NOT_ALLOWED");
    case EXCEPTION:
        return QStringLiteral("EXCEPTION");
    case TIMEOUT:
        return QStringLiteral("TIMEOUT");
    default:
        return QStringLiteral("UNKNOWN");
    }
}

QString MainWindow::robotTypeName(int type) const
{
    switch (type) {
    case 1:
        return QStringLiteral("六轴串联");
    case 2:
        return QStringLiteral("四轴 SCARA");
    case 3:
        return QStringLiteral("四轴码垛");
    case 4:
        return QStringLiteral("四轴串联");
    case 5:
        return QStringLiteral("单轴");
    case 6:
        return QStringLiteral("五轴串联");
    case 7:
        return QStringLiteral("六轴协作");
    case 8:
        return QStringLiteral("二轴 SCARA");
    case 9:
        return QStringLiteral("三轴 SCARA");
    case 10:
        return QStringLiteral("三轴直角");
    case 11:
        return QStringLiteral("三轴异构");
    case 12:
        return QStringLiteral("七轴串联");
    case 13:
        return QStringLiteral("SCARA 异构");
    case 14:
        return QStringLiteral("四轴码垛丝杆");
    default:
        return QStringLiteral("未知类型(%1)").arg(type);
    }
}

std::array<double, 7> MainWindow::targetPoseFromInputs() const
{
    std::array<double, 7> pose = {};
    for (int i = 0; i < kVisibleAxisCount; ++i) {
        pose[i] = m_targetPos[i]->value();
    }
    clearHiddenAxis(pose);
    return pose;
}

QVector3D MainWindow::poseToVector3D(const std::array<double, 7>& pose)
{
    return QVector3D(static_cast<float>(pose[0]),
                     static_cast<float>(pose[1]),
                     static_cast<float>(pose[2]));
}

void MainWindow::onConnectClicked()
{
    const QString ip = m_ipEdit->text().trimmed();
    const QString port = m_portEdit->text().trimmed();

    if (ip.isEmpty() || port.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("参数错误"), QStringLiteral("请输入有效的 IP 地址和端口号。"));
        return;
    }

    appendLog(QStringLiteral("正在连接 %1:%2 ...").arg(ip, port));

    m_socketFd = connect_robot(ip.toStdString(), port.toStdString());
    if (m_socketFd == -1) {
        appendLog(QStringLiteral("<span style='color:#f38ba8;'>连接控制器 %1:%2 失败（connect_robot 返回 -1）。</span>").arg(ip, port));

        const auto choice = QMessageBox::warning(
            this,
            QStringLiteral("连接失败"),
            QStringLiteral("无法连接到控制器 %1:%2。\n\n"
                           "• 请确认控制器已上电、网线连通、IP 与端口（通常为 6001）正确；\n"
                           "• 点击 [是] 进入演示模式（仅本地预览，不会下发任何 SDK 指令，"
                           "全局变量读写/运动指令等均不会实际生效）；\n"
                           "• 点击 [否] 取消并返回未连接状态。").arg(ip, port),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (choice != QMessageBox::Yes) {
            m_connected = false;
            m_demoMode = false;
            m_socketFd = -1;
            setConnectionUi(false);
            updateConnectionStatusLabel(QStringLiteral("● 未连接"), false);
            refreshGlobalVariantModeLabel();
            notifyAutoCalibConnection();
            return;
        }

        m_connected = true;
        m_demoMode = true;
        resetDemoState();
        setConnectionUi(true);
        updateConnectionStatusLabel(QStringLiteral("⚠ 演示模式 %1:%2（未连接真机）").arg(ip, port), false);
        m_updateTimer->start(250);
        m_previewTimer->start();
        refreshPositionDisplay();
        refreshPreview(false);
        refreshJobStatus();
        refreshStatusSummary();
        refreshGlobalVariantModeLabel();
        notifyAutoCalibConnection();
        notifyAutoCalibPose();
        appendLog(QStringLiteral("<span style='color:#f9e2af;'>已进入演示模式，可预览机械臂运动与轨迹；SDK 接口不会真正下发。</span>"));
        return;
    }

    m_connected = true;
    m_demoMode = false;
    setConnectionUi(true);
    updateConnectionStatusLabel(QStringLiteral("● 已连接 %1:%2").arg(ip, port), true);
    pollRobotState();
    refreshPositionDisplay();
    refreshPreview(false);
    refreshJobStatus();
    refreshStatusSummary();
    refreshGlobalVariantModeLabel();
    notifyAutoCalibConnection();
    notifyAutoCalibPose();
    m_updateTimer->start();
    m_previewTimer->start();

    appendLog(QStringLiteral("<span style='color:#a6e3a1;'>已成功连接到控制器 %1:%2</span>").arg(ip, port));
}

void MainWindow::onDisconnectClicked()
{
    if (!m_demoMode && m_socketFd != -1) {
        disconnect_robot(m_socketFd);
    }

    stopTimers();
    m_connected = false;
    m_demoMode = false;
    m_socketFd = -1;
    m_activeJogAxis = -1;
    m_activeJogDirection = 0;
    m_previewMotionMode = PreviewMotionMode::None;

    setConnectionUi(false);
    updateConnectionStatusLabel(QStringLiteral("● 未连接"), false);
    refreshPreview(false);
    refreshJobStatus();
    refreshStatusSummary();
    refreshGlobalVariantModeLabel();
    notifyAutoCalibConnection();

    appendLog(QStringLiteral("<span style='color:#f38ba8;'>已断开与控制器的连接。</span>"));
}

void MainWindow::onServoReady()
{
    if (!m_connected) {
        return;
    }

    if (m_demoMode) {
        m_servoState = 1;
        refreshStatusSummary();
        appendLog(QStringLiteral("演示模式：伺服切换到就绪状态。"));
        return;
    }

    const Result result = set_servo_state(m_socketFd, 1);
    appendLog(QStringLiteral("设置伺服就绪 -> %1").arg(resultToString(result)));
}

void MainWindow::onServoPowerOn()
{
    if (!m_connected) {
        return;
    }

    if (m_demoMode) {
        m_servoState = 3;
        refreshStatusSummary();
        appendLog(QStringLiteral("<span style='color:#a6e3a1;'>演示模式：机械臂已上电。</span>"));
        return;
    }

    const Result result = set_servo_poweron(m_socketFd);
    appendLog(QStringLiteral("伺服上电 -> %1").arg(resultToString(result)));
}

void MainWindow::onServoPowerOff()
{
    if (!m_connected) {
        return;
    }

    if (m_demoMode) {
        m_servoState = 0;
        m_runState = 0;
        refreshStatusSummary();
        appendLog(QStringLiteral("<span style='color:#f9e2af;'>演示模式：机械臂已下电。</span>"));
        return;
    }

    const Result result = set_servo_poweroff(m_socketFd);
    appendLog(QStringLiteral("伺服下电 -> %1").arg(resultToString(result)));
}

void MainWindow::onClearError()
{
    if (!m_connected) {
        return;
    }

    if (m_demoMode) {
        m_servoState = 1;
        refreshStatusSummary();
        appendLog(QStringLiteral("演示模式：已清除报警状态。"));
        return;
    }

    const Result result = clear_error(m_socketFd);
    appendLog(QStringLiteral("清除报警 -> %1").arg(resultToString(result)));
}

void MainWindow::onModeChanged(int index)
{
    const QStringList modeNames = {QStringLiteral("示教"), QStringLiteral("远程"), QStringLiteral("运行")};
    appendLog(QStringLiteral("切换模式: %1").arg(modeNames.value(index)));

    if (m_connected && !m_demoMode) {
        const Result result = set_current_mode(m_socketFd, index);
        if (result != SUCCESS) {
            appendLog(QStringLiteral("<span style='color:#f38ba8;'>模式切换失败: %1</span>").arg(resultToString(result)));
        }
    }
}

void MainWindow::onCoordChanged(int index)
{
    Q_UNUSED(index);
    refreshPositionHeaders();
    refreshPositionDisplay();

    const QStringList coordNames = {QStringLiteral("关节"), QStringLiteral("直角"), QStringLiteral("工具"), QStringLiteral("用户")};
    appendLog(QStringLiteral("切换控制坐标系: %1").arg(coordNames.value(m_coordCombo->currentIndex())));

    if (m_connected && !m_demoMode) {
        const Result result = set_current_coord(m_socketFd, m_coordCombo->currentIndex());
        if (result != SUCCESS) {
            appendLog(QStringLiteral("<span style='color:#f38ba8;'>坐标系切换失败: %1</span>").arg(resultToString(result)));
        }
    }
}

void MainWindow::onSpeedSliderChanged(int value)
{
    Q_UNUSED(value);
}

void MainWindow::onSpeedApply()
{
    const int speed = m_speedSpin->value();
    appendLog(QStringLiteral("设置速度: %1%").arg(speed));

    if (m_connected && !m_demoMode) {
        const Result result = set_speed(m_socketFd, speed);
        if (result != SUCCESS) {
            appendLog(QStringLiteral("<span style='color:#f38ba8;'>速度设置失败: %1</span>").arg(resultToString(result)));
        }
    }
}

void MainWindow::onJogStart()
{
    if (!m_connected) {
        return;
    }

    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (!button) {
        return;
    }

    const int axis = button->property("axis").toInt();
    const bool dir = button->property("dir").toBool();
    appendLog(QStringLiteral("开始点动: 轴 %1 方向 %2").arg(axis + 1).arg(dir ? "+" : "-"));

    if (m_demoMode) {
        m_activeJogAxis = axis;
        m_activeJogDirection = dir ? 1 : -1;
        m_previewMotionMode = PreviewMotionMode::None;
        return;
    }

    const Result result = robot_start_jogging(m_socketFd, axis + 1, dir);
    if (result != SUCCESS) {
        appendLog(QStringLiteral("<span style='color:#f38ba8;'>点动启动失败: %1</span>").arg(resultToString(result)));
    }
}

void MainWindow::onJogStop()
{
    if (!m_connected) {
        return;
    }

    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (!button) {
        return;
    }

    const int axis = button->property("axis").toInt();
    if (m_demoMode) {
        m_activeJogAxis = -1;
        m_activeJogDirection = 0;
        m_runState = 0;
        return;
    }

    const Result result = robot_stop_jogging(m_socketFd, axis + 1);
    if (result != SUCCESS) {
        appendLog(QStringLiteral("<span style='color:#f38ba8;'>点动停止失败: %1</span>").arg(resultToString(result)));
    }
}

void MainWindow::onMoveJ()
{
    if (!m_connected) {
        return;
    }

    const std::array<double, 7> targetPose = targetPoseFromInputs();

    appendLog(QStringLiteral("MOVJ -> [%1] 速度:%2 加速度:%3 减速度:%4")
                  .arg(formatVisiblePose(targetPose))
                  .arg(m_moveVelSpin->value())
                  .arg(m_moveAccSpin->value())
                  .arg(m_moveDecSpin->value()));

    if (m_demoMode) {
        if (m_coordCombo->currentIndex() == 0) {
            startJointAnimation(targetPose, m_moveVelSpin->value());
        } else {
            startJointAnimation(RobotPreviewWidget::approximateJointsFromTcp(targetPose), m_moveVelSpin->value());
        }
        return;
    }

    const Result result = robot_movej(m_socketFd, buildMoveCommand());
    if (result != SUCCESS) {
        appendLog(QStringLiteral("<span style='color:#f38ba8;'>MOVJ 执行失败: %1</span>").arg(resultToString(result)));
    }
}

void MainWindow::onMoveL()
{
    if (!m_connected) {
        return;
    }

    const std::array<double, 7> targetPose = targetPoseFromInputs();

    appendLog(QStringLiteral("MOVL -> [%1] 速度:%2 加速度:%3 减速度:%4")
                  .arg(formatVisiblePose(targetPose))
                  .arg(m_moveVelSpin->value())
                  .arg(m_moveAccSpin->value())
                  .arg(m_moveDecSpin->value()));

    if (m_demoMode) {
        if (m_coordCombo->currentIndex() == 0) {
            startJointAnimation(targetPose, m_moveVelSpin->value());
        } else {
            startLinearAnimation(targetPose, m_moveVelSpin->value());
        }
        return;
    }

    const Result result = robot_movel(m_socketFd, buildMoveCommand());
    if (result != SUCCESS) {
        appendLog(QStringLiteral("<span style='color:#f38ba8;'>MOVL 执行失败: %1</span>").arg(resultToString(result)));
    }
}

void MainWindow::onGoHome()
{
    if (!m_connected) {
        return;
    }

    appendLog(QStringLiteral("执行回零点..."));
    if (m_demoMode) {
        startJointAnimation({0.0, 42.0, 58.0, -18.0, 10.0, -22.0, 0.0}, 75);
        return;
    }

    const Result result = robot_go_home(m_socketFd);
    if (result != SUCCESS) {
        appendLog(QStringLiteral("<span style='color:#f38ba8;'>回零点失败: %1</span>").arg(resultToString(result)));
    }
}

void MainWindow::onGoReset()
{
    if (!m_connected) {
        return;
    }

    appendLog(QStringLiteral("执行复位点..."));
    if (m_demoMode) {
        startJointAnimation({25.0, 18.0, 72.0, -12.0, 22.0, -10.0, 0.0}, 70);
        return;
    }

    const Result result = robot_go_to_reset_position(m_socketFd);
    if (result != SUCCESS) {
        appendLog(QStringLiteral("<span style='color:#f38ba8;'>复位点失败: %1</span>").arg(resultToString(result)));
    }
}

void MainWindow::onJobRun()
{
    if (!ensureJobControllerReady(QStringLiteral("运行作业"))) {
        return;
    }

    {
    const QString name = normalizedJobName();
    if (name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("参数错误"), QStringLiteral("请输入作业文件名。"));
        return;
    }

    const int times = m_jobRunTimesSpin->value();
    const Result timesResult = job_run_times(m_socketFd, times);
    if (timesResult != SUCCESS) {
        appendLog(QStringLiteral("<span style='color:#f38ba8;'>设置作业运行次数失败: %1</span>").arg(resultToString(timesResult)));
        return;
    }

    const Result runResult = job_run(m_socketFd, name.toStdString());
    appendLog(QStringLiteral("运行作业: %1 (次数:%2) -> %3")
                  .arg(name,
                       times == 0 ? QStringLiteral("无限") : QString::number(times),
                       resultToString(runResult)));
    refreshJobStatus();
    return;

    }
    if (!m_connected) {
        return;
    }

    const QString name = m_jobNameEdit->text().trimmed();
    if (name.isEmpty()) {
        return;
    }

    const int times = m_jobRunTimesSpin->value();
    appendLog(QStringLiteral("运行作业: %1 (次数:%2)")
                  .arg(name, times == 0 ? QStringLiteral("无限") : QString::number(times)));
}

void MainWindow::onJobPause()
{
    if (!ensureJobControllerReady(QStringLiteral("暂停作业"))) {
        return;
    }

    const Result result = job_pause(m_socketFd);
    appendLog(QStringLiteral("暂停作业 -> %1").arg(resultToString(result)));
    refreshJobStatus();
    return;

    if (!m_connected) {
        return;
    }
    appendLog(QStringLiteral("暂停作业"));
}

void MainWindow::onJobContinue()
{
    if (!ensureJobControllerReady(QStringLiteral("继续作业"))) {
        return;
    }

    const Result result = job_continue(m_socketFd);
    appendLog(QStringLiteral("继续作业 -> %1").arg(resultToString(result)));
    refreshJobStatus();
    return;

    if (!m_connected) {
        return;
    }
    appendLog(QStringLiteral("继续作业"));
}

void MainWindow::onJobStop()
{
    if (!ensureJobControllerReady(QStringLiteral("停止作业"))) {
        return;
    }

    const Result result = job_stop(m_socketFd);
    appendLog(QStringLiteral("<span style='color:#f38ba8;'>停止作业 -> %1</span>").arg(resultToString(result)));
    refreshJobStatus();
    return;

    if (!m_connected) {
        return;
    }
    appendLog(QStringLiteral("<span style='color:#f38ba8;'>停止作业</span>"));
}

void MainWindow::onJobCreate()
{
    if (!ensureJobControllerReady(QStringLiteral("新建作业"))) {
        return;
    }

    {
    const QString name = normalizedJobName();
    if (name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("参数错误"), QStringLiteral("请输入作业文件名。"));
        return;
    }

    const Result result = job_create(m_socketFd, name.toStdString());
    appendLog(QStringLiteral("新建作业文件: %1 -> %2").arg(name, resultToString(result)));
    refreshJobStatus();
    return;
    }

    if (!m_connected) {
        return;
    }

    const QString name = m_jobNameEdit->text().trimmed();
    if (name.isEmpty()) {
        return;
    }
    appendLog(QStringLiteral("新建作业文件: %1").arg(name));
}

void MainWindow::onJobOpen()
{
    if (!ensureJobControllerReady(QStringLiteral("打开作业"))) {
        return;
    }

    {
    const QString name = normalizedJobName();
    if (name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("参数错误"), QStringLiteral("请输入作业文件名。"));
        return;
    }

    const Result result = job_open(m_socketFd, name.toStdString());
    appendLog(QStringLiteral("打开作业文件: %1 -> %2").arg(name, resultToString(result)));
    refreshJobStatus();
    return;
    }

    if (!m_connected) {
        return;
    }

    const QString name = m_jobNameEdit->text().trimmed();
    if (name.isEmpty()) {
        return;
    }
    appendLog(QStringLiteral("打开作业文件: %1").arg(name));
}

void MainWindow::onJobDelete()
{
    if (!ensureJobControllerReady(QStringLiteral("删除作业"))) {
        return;
    }

    {
    const QString name = normalizedJobName();
    if (name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("参数错误"), QStringLiteral("请输入作业文件名。"));
        return;
    }

    if (QMessageBox::question(this,
                              QStringLiteral("确认删除"),
                              QStringLiteral("确定要删除作业文件 \"%1\" 吗？").arg(name)) != QMessageBox::Yes) {
        return;
    }

    const Result result = job_delete(m_socketFd, name.toStdString());
    appendLog(QStringLiteral("<span style='color:#f38ba8;'>删除作业文件: %1 -> %2</span>").arg(name, resultToString(result)));
    refreshJobStatus();
    return;
    }

    if (!m_connected) {
        return;
    }

    const QString name = m_jobNameEdit->text().trimmed();
    if (name.isEmpty()) {
        return;
    }

    if (QMessageBox::question(this,
                              QStringLiteral("确认删除"),
                              QStringLiteral("确定要删除作业文件 \"%1\" 吗？").arg(name)) != QMessageBox::Yes) {
        return;
    }

    appendLog(QStringLiteral("<span style='color:#f38ba8;'>删除作业文件: %1</span>").arg(name));
}

void MainWindow::onSetDigitalOutput()
{
    if (!m_connected) {
        return;
    }

    appendLog(QStringLiteral("设置数字输出: DO%1 = %2")
                  .arg(m_doPortSpin->value())
                  .arg(m_doValueCombo->currentIndex()));
}

void MainWindow::onRefreshIO()
{
    if (!m_connected) {
        return;
    }

    appendLog(QStringLiteral("刷新 IO 状态..."));
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 2; ++c) {
            m_diTable->item(r, c * 2 + 1)->setText(QStringLiteral("0"));
            m_doTable->item(r, c * 2 + 1)->setText(QStringLiteral("0"));
        }
    }
}

void MainWindow::onSetGlobalVariant()
{
    const QString varName = m_globalVarNameEdit->text().trimmed();
    if (varName.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("参数错误"), QStringLiteral("请输入全局变量名，例如 GI001 / GD001 / GB001。"));
        return;
    }

    const QString prefix = varName.left(2).toUpper();
    double varValue = m_globalVarValueSpin->value();
    int valueDecimals = 6;

    if (prefix == QStringLiteral("GB")) {
        varValue = (varValue >= 0.5) ? 1.0 : 0.0;
        valueDecimals = 0;
    } else if (prefix == QStringLiteral("GI")) {
        varValue = std::trunc(varValue);
        valueDecimals = 0;
    }

    const QString valueText = QString::number(varValue, 'f', valueDecimals);

    if (!m_connected) {
        appendLog(QStringLiteral("<span style='color:#f9e2af;'>设置全局变量失败：请先连接控制器。</span>"));
        return;
    }

    if (m_demoMode || m_socketFd == -1) {
        m_globalVarResultLabel->setText(QStringLiteral("演示模式：%1 = %2").arg(varName, valueText));
        appendLog(QStringLiteral("<span style='color:#f9e2af;'>演示模式：模拟设置全局变量 %1 = %2</span>")
                      .arg(varName, valueText));
        return;
    }

    const Result result = set_global_variant(m_socketFd, varName.toStdString(), varValue);
    if (result == SUCCESS) {
        m_globalVarResultLabel->setText(QStringLiteral("已写入: %1 = %2").arg(varName, valueText));
        appendLog(QStringLiteral("<span style='color:#a6e3a1;'>设置全局变量 %1 = %2 -> %3</span>")
                      .arg(varName, valueText, resultToString(result)));
    } else {
        appendLog(QStringLiteral("<span style='color:#f38ba8;'>设置全局变量 %1 失败: %2</span>")
                      .arg(varName, resultToString(result)));
    }
}

void MainWindow::onGetGlobalVariant()
{
    const QString varName = m_globalVarNameEdit->text().trimmed();
    if (varName.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("参数错误"), QStringLiteral("请输入全局变量名，例如 GI001 / GD001 / GB001。"));
        return;
    }

    const QString prefix = varName.left(2).toUpper();
    const int valueDecimals = (prefix == QStringLiteral("GB") || prefix == QStringLiteral("GI")) ? 0 : 6;

    if (!m_connected) {
        appendLog(QStringLiteral("<span style='color:#f9e2af;'>获取全局变量失败：请先连接控制器。</span>"));
        return;
    }

    if (m_demoMode || m_socketFd == -1) {
        const double demoValue = m_globalVarValueSpin->value();
        const QString demoText = QString::number(demoValue, 'f', valueDecimals);
        m_globalVarResultLabel->setText(QStringLiteral("演示模式：%1 = %2").arg(varName, demoText));
        appendLog(QStringLiteral("<span style='color:#f9e2af;'>演示模式：返回当前显示值 %1 = %2</span>")
                      .arg(varName, demoText));
        return;
    }

    double value = 0.0;
    const Result result = get_global_variant(m_socketFd, varName.toStdString(), value);
    if (result == SUCCESS) {
        if (prefix == QStringLiteral("GB")) {
            value = (value >= 0.5) ? 1.0 : 0.0;
        } else if (prefix == QStringLiteral("GI")) {
            value = std::trunc(value);
        }
        m_globalVarValueSpin->setValue(value);
        const QString valueText = QString::number(value, 'f', valueDecimals);
        m_globalVarResultLabel->setText(QStringLiteral("已读取: %1 = %2").arg(varName, valueText));
        appendLog(QStringLiteral("<span style='color:#a6e3a1;'>获取全局变量 %1 = %2</span>")
                      .arg(varName, valueText));
    } else {
        appendLog(QStringLiteral("<span style='color:#f38ba8;'>获取全局变量 %1 失败: %2</span>")
                      .arg(varName, resultToString(result)));
    }
}

void MainWindow::onToggleWindGrinder()
{
    if (!m_connected) {
        appendLog(QStringLiteral("<span style='color:#f9e2af;'>操作失败：请先连接控制器。</span>"));
        return;
    }

    const bool turnOn = !m_windGrinderOn;
    const int value = turnOn ? 1 : 0;

    if (m_demoMode || m_socketFd == -1) {
        m_windGrinderOn = turnOn;
        m_windGrinderBtn->setText(turnOn ? QStringLiteral("关闭风磨") : QStringLiteral("启动风磨"));
        m_windGrinderBtn->setProperty("styleClass", turnOn ? "danger" : "success");
        m_windGrinderBtn->style()->unpolish(m_windGrinderBtn);
        m_windGrinderBtn->style()->polish(m_windGrinderBtn);
        appendLog(QStringLiteral("<span style='color:#f9e2af;'>演示模式：模拟 set_digital_output(port=6, value=%1)</span>").arg(value));
        return;
    }

    const Result result = set_digital_output(m_socketFd, 6, value);
    if (result == SUCCESS) {
        m_windGrinderOn = turnOn;
        m_windGrinderBtn->setText(turnOn ? QStringLiteral("关闭风磨") : QStringLiteral("启动风磨"));
        m_windGrinderBtn->setProperty("styleClass", turnOn ? "danger" : "success");
        m_windGrinderBtn->style()->unpolish(m_windGrinderBtn);
        m_windGrinderBtn->style()->polish(m_windGrinderBtn);
        appendLog(QStringLiteral("<span style='color:#a6e3a1;'>%1风磨 -> set_digital_output(port=6, value=%2) -> SUCCESS</span>")
                      .arg(turnOn ? QStringLiteral("启动") : QStringLiteral("关闭")).arg(value));
    } else {
        appendLog(QStringLiteral("<span style='color:#f38ba8;'>%1风磨失败: set_digital_output(port=6, value=%2) -> %3</span>")
                      .arg(turnOn ? QStringLiteral("启动") : QStringLiteral("关闭")).arg(value).arg(resultToString(result)));
    }
}

void MainWindow::onTimerUpdate()
{
    if (!m_connected) {
        return;
    }

    if (!m_demoMode) {
        pollRobotState();
    }

    refreshPositionDisplay();
    refreshJobStatus();
    refreshStatusSummary();
    notifyAutoCalibPose();
}

void MainWindow::onPreviewFrame()
{
    if (!m_connected) {
        return;
    }

    if (m_demoMode) {
        stepDemoMotion();
        refreshPositionDisplay();
        refreshStatusSummary();
        notifyAutoCalibPose();
    }

    refreshPreview(true);
}

void MainWindow::onClearPreviewTrace()
{
    m_robotPreview->clearTrace();
    refreshPreview(false);
    appendLog(QStringLiteral("已清空末端运动轨迹。"));
}

QWidget* MainWindow::createAutoCalibrationPanel()
{
    m_autoCalib = new AutoCalibrationWidget();
    connect(m_autoCalib, &AutoCalibrationWidget::log, this, &MainWindow::appendLog);
    return m_autoCalib;
}

void MainWindow::notifyAutoCalibConnection()
{
    if (!m_autoCalib) return;
    m_autoCalib->setConnectionState(m_connected, m_demoMode, m_socketFd);
}

void MainWindow::notifyAutoCalibPose()
{
    if (!m_autoCalib) return;
    m_autoCalib->updateRobotState(m_jointPose, m_cartesianPose, m_runState);
}
