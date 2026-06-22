#include "mainwindow.h"

#include "workpieceselectionwidget.h"

#include "robot_sdk/cpp/robot_arm_actions.h"
#include "robot_sdk/cpp/robot_adapter.h"
#include "robot_sdk/cpp/interface/nrc_interface.h"
#include "robot_sdk/cpp/interface/nrc_io.h"
#include "robot_sdk/cpp/interface/nrc_job_operate.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMetaObject>
#include <QMessageBox>
#include <QIODevice>
#include <QHostAddress>
#include <QRegularExpression>
#include <QScreen>
#include <QSettings>
#include <QSizePolicy>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidgetItem>
#include <QTextCursor>
#include <QTextStream>
#include <QThread>
#include <QTcpSocket>
#include <QVBoxLayout>

#include <string>
#include <vector>

namespace {

constexpr auto kHuichuanConnectionConfigFile = "robot_connection_huichuan.ini";
constexpr auto kYinglianConnectionConfigFile = "robot_connection_yinglian.ini";
constexpr auto kConnectionHistoryConfigFile = "robot_connection_history.ini";
constexpr auto kGlobalVarPresetFile = "global_var_presets.txt";
constexpr int kWindGrinderPort = 6;
constexpr int kWorkpiecePageWidth = 1080;
constexpr int kLogPanelWidth = 380;
constexpr int kTcpProbeTimeoutMs = 1000;

QString connectionConfigPathForBrand(const QString& brand)
{
    const QString fileName = brand == QStringLiteral("yinglian")
        ? QString::fromLatin1(kYinglianConnectionConfigFile)
        : QString::fromLatin1(kHuichuanConnectionConfigFile);
    const QString appConfigPath = QDir(QApplication::applicationDirPath()).filePath(fileName);
    if (QFileInfo::exists(appConfigPath)) {
        return appConfigPath;
    }

    const QString currentConfigPath = QDir::current().filePath(fileName);
    if (QFileInfo::exists(currentConfigPath)) {
        return currentConfigPath;
    }

    return appConfigPath;
}

QString connectionHistoryConfigPathForAppDir()
{
    const QString fileName = QString::fromLatin1(kConnectionHistoryConfigFile);
    return QDir(QApplication::applicationDirPath()).filePath(fileName);
}

QString connectionHistoryConfigPathForCurrentDir()
{
    const QString fileName = QString::fromLatin1(kConnectionHistoryConfigFile);
    return QDir::current().filePath(fileName);
}

QString latestExistingHistoryConfigPath()
{
    const QString appPath = connectionHistoryConfigPathForAppDir();
    const QString currentPath = connectionHistoryConfigPathForCurrentDir();
    const bool appExists = QFileInfo::exists(appPath);
    const bool currentExists = QFileInfo::exists(currentPath);

    if (appExists && currentExists) {
        const QFileInfo appInfo(appPath);
        const QFileInfo currentInfo(currentPath);
        return appInfo.lastModified() >= currentInfo.lastModified() ? appPath : currentPath;
    }
    if (appExists) {
        return appPath;
    }
    if (currentExists) {
        return currentPath;
    }
    return appPath;
}

QString globalVarPresetPath()
{
    const QString fileName = QString::fromLatin1(kGlobalVarPresetFile);
    const QString appPath = QDir(QApplication::applicationDirPath()).filePath(fileName);
    if (QFileInfo::exists(appPath)) {
        return appPath;
    }

    const QString currentPath = QDir::current().filePath(fileName);
    if (QFileInfo::exists(currentPath)) {
        return currentPath;
    }

    return appPath;
}

QString logFilePathForToday()
{
    const QString fileName = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")) + QStringLiteral(".txt");
    const QString logDirName = QStringLiteral("logs");
    const QString appLogDir = QDir(QApplication::applicationDirPath()).filePath(logDirName);
    QDir appDir(appLogDir);
    if (appDir.exists() || appDir.mkpath(QStringLiteral("."))) {
        return appDir.filePath(fileName);
    }

    const QString currentLogDir = QDir::current().filePath(logDirName);
    QDir currentDir(currentLogDir);
    if (currentDir.exists() || currentDir.mkpath(QStringLiteral("."))) {
        return currentDir.filePath(fileName);
    }

    return QDir::current().filePath(fileName);
}

QString probeTcpEndpoint(const QString& ip, const QString& port)
{
    bool portOk = false;
    const quint16 portValue = port.toUShort(&portOk);
    if (!portOk || portValue == 0) {
        return QStringLiteral("TCP 探测未执行: 端口无效 (%1)。").arg(port);
    }

    QTcpSocket socket;
    socket.connectToHost(QHostAddress(ip), portValue);
    if (socket.waitForConnected(kTcpProbeTimeoutMs)) {
        socket.disconnectFromHost();
        return QStringLiteral("TCP 探测结果: %1:%2 端口可达，说明基础网络/端口连通；当前失败更可能发生在汇川 SDK 协议初始化、控制器状态、连接占用或 SDK 版本匹配阶段。")
            .arg(ip, port);
    }

    return QStringLiteral("TCP 探测结果: %1:%2 端口不可达或 %3 ms 内无响应，优先检查控制器 IP、端口、网段、网线/交换机、防火墙和控制器以太网服务。QtSocketError=%4, errorString=%5")
        .arg(ip, port)
        .arg(kTcpProbeTimeoutMs)
        .arg(static_cast<int>(socket.error()))
        .arg(socket.errorString());
}

QString brandTextForRobotBrand(RobotBrand brand)
{
    return brand == RobotBrand::Yinglian ? QStringLiteral("盈联") : QStringLiteral("汇川");
}

QString brandNameForRobotBrand(RobotBrand brand)
{
    return brand == RobotBrand::Yinglian ? QStringLiteral("yinglian") : QStringLiteral("huichuan");
}

QString connectionSummaryText(const QString& brandText, const QString& ip, const QString& port)
{
    if (ip.isEmpty() || port.isEmpty()) {
        return QStringLiteral("--");
    }
    return QStringLiteral("%1 %2:%3").arg(brandText, ip, port);
}

bool readConnectionSettingsForBrand(const QString& brand, QString& ipOut, QString& portOut)
{
    ipOut.clear();
    portOut.clear();
    const QString configPath = connectionConfigPathForBrand(brand);
    if (!QFileInfo::exists(configPath)) {
        return false;
    }

    QSettings settings(configPath, QSettings::IniFormat);
    ipOut = settings.value(QStringLiteral("connection/ip")).toString().trimmed();
    portOut = settings.value(QStringLiteral("connection/port")).toString().trimmed();
    return !ipOut.isEmpty() && !portOut.isEmpty();
}

bool isValidConnectionEndpoint(const QString& ip, const QString& port)
{
    bool portOk = false;
    const quint16 portValue = port.toUShort(&portOk);
    if (!portOk || portValue == 0) {
        return false;
    }

    QHostAddress address;
    return address.setAddress(ip);
}

QString readLastSuccessfulBrand()
{
    const QString configPath = latestExistingHistoryConfigPath();
    if (!QFileInfo::exists(configPath)) {
        return QString();
    }

    QSettings settings(configPath, QSettings::IniFormat);
    const QString brand = settings.value(QStringLiteral("connection/last_brand")).toString().trimmed().toLower();
    if (brand == QStringLiteral("yinglian") || brand == QStringLiteral("huichuan")) {
        return brand;
    }
    return QString();
}

void saveLastSuccessfulBrand(const QString& brand)
{
    const QString normalizedBrand = brand.trimmed().toLower();
    if (normalizedBrand != QStringLiteral("yinglian") && normalizedBrand != QStringLiteral("huichuan")) {
        return;
    }

    const QString appPath = connectionHistoryConfigPathForAppDir();
    {
        QSettings settings(appPath, QSettings::IniFormat);
        settings.setValue(QStringLiteral("connection/last_brand"), normalizedBrand);
    }

    const QString currentPath = connectionHistoryConfigPathForCurrentDir();
    if (currentPath != appPath) {
        QSettings settings(currentPath, QSettings::IniFormat);
        settings.setValue(QStringLiteral("connection/last_brand"), normalizedBrand);
    }
}

struct JobStartCheckItem
{
    std::string name;
    Result result = EXCEPTION;
    std::string detail;
};

Result queryCurrentJobProgram(SOCKETFD socketFd, std::string& programOut)
{
    programOut.clear();
    return robot_job_get_current_file(socketFd, programOut);
}

QString normalizedProgramNameFromSdkValue(const std::string& value)
{
    const QString programValue = QString::fromLocal8Bit(value.data(), static_cast<int>(value.size())).trimmed();
    if (programValue.isEmpty()) {
        return QString();
    }

    const QFileInfo programFile(programValue);
    QString programName;
    if (programValue.contains(QLatin1Char('/')) || programValue.contains(QLatin1Char('\\'))) {
        programName = programFile.dir().dirName();
    }
    if (programName.isEmpty() || programName == QStringLiteral(".")) {
        programName = programFile.completeBaseName();
    }
    if (programName.isEmpty()) {
        programName = programValue;
    }
    return programName;
}

QString programNameWithoutFileType(const QString& value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty() || trimmed == QStringLiteral("NOT_SEGMENT")) {
        return QString();
    }

    const QFileInfo fileInfo(trimmed);
    QString name = trimmed.contains(QLatin1Char('/')) || trimmed.contains(QLatin1Char('\\'))
        ? fileInfo.completeBaseName()
        : trimmed;
    if (name.endsWith(QStringLiteral(".JBR"), Qt::CaseInsensitive)) {
        name.chop(4);
    } else {
        const QFileInfo plainFile(name);
        if (!plainFile.suffix().isEmpty()) {
            name = plainFile.completeBaseName();
        }
    }
    return name.trimmed();
}

QString huichuanConfiguredActiveProgramName()
{
    QSettings settings(connectionConfigPathForBrand(QStringLiteral("huichuan")), QSettings::IniFormat);
    return settings.value(QStringLiteral("active_program/name")).toString().trimmed();
}

bool huichuanConfiguredWriteVariable(QString& nameOut)
{
    QSettings settings(connectionConfigPathForBrand(QStringLiteral("huichuan")), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("global_variable"));
    const QStringList groups = settings.childGroups();
    for (const QString& group : groups) {
        settings.beginGroup(group);
        const QString mode = settings.value(QStringLiteral("mode")).toString().trimmed().toLower();
        const QString name = settings.value(QStringLiteral("name")).toString().trimmed().toUpper();
        settings.endGroup();
        if (mode == QStringLiteral("write") && !name.isEmpty()) {
            nameOut = name;
            settings.endGroup();
            return true;
        }
    }
    settings.endGroup();
    nameOut.clear();
    return false;
}

Result diagnoseJobStartConditions(SOCKETFD socketFd, std::vector<JobStartCheckItem>& itemsOut)
{
    itemsOut.clear();
    Result firstFailure = SUCCESS;

    auto addItem = [&itemsOut, &firstFailure](const std::string& name, Result result, const std::string& detail) {
        itemsOut.push_back({name, result, detail});
        if (firstFailure == SUCCESS && result != SUCCESS) {
            firstFailure = result;
        }
    };

    const Result connectionResult = robot_get_connection_status(socketFd);
    addItem("Connection", connectionResult, connectionResult == SUCCESS ? "connected" : "not connected");
    if (connectionResult != SUCCESS) {
        return firstFailure;
    }

    int mode = 0;
    Result result = robot_get_current_mode(socketFd, mode);
    addItem("Mode", result == SUCCESS && mode == 2 ? SUCCESS : OPERATION_NOT_ALLOWED,
            result == SUCCESS ? "mode=" + std::to_string(mode) : "robot_get_current_mode failed");

    int servoState = 0;
    result = robot_get_servo_state(socketFd, servoState);
    addItem("Servo state", result == SUCCESS && servoState == 3 ? SUCCESS : OPERATION_NOT_ALLOWED,
            result == SUCCESS ? "state=" + std::to_string(servoState) : "robot_get_servo_state failed");

    int runState = 0;
    result = robot_get_robot_running_state(socketFd, runState);
    addItem("Run state", result, result == SUCCESS ? "state=" + std::to_string(runState) : "robot_get_robot_running_state failed");

    std::string currentProgram;
    result = queryCurrentJobProgram(socketFd, currentProgram);
    addItem("Current job", result,
            result == SUCCESS ? "job=" + currentProgram : "robot_job_get_current_file failed");

    return firstFailure;
}

QString servoStateText(int state)
{
    switch (state) {
    case 0: return QStringLiteral("停止");
    case 1: return QStringLiteral("就绪");
    case 2: return QStringLiteral("报警");
    case 3: return QStringLiteral("运行");
    default: return QStringLiteral("--");
    }
}

QString runStateText(int state)
{
    switch (state) {
    case 0: return QStringLiteral("停止");
    case 1: return QStringLiteral("暂停");
    case 2: return QStringLiteral("运行中");
    default: return QStringLiteral("--");
    }
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("机器人控制界面"));

    if (QScreen *screen = QApplication::primaryScreen()) {
        const QRect screenRect = screen->availableGeometry();
        resize(qMin(1500, screenRect.width() - 120), qMin(960, screenRect.height() - 120));
    } else {
        resize(1400, 900);
    }

    setupUI();
    setupStyleSheet();
    loadConnectionSettings();

    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(250);
    connect(m_updateTimer, &QTimer::timeout, this, &MainWindow::onTimerUpdate);

    refreshStatusSummary();
    if (m_logEdit) {
        m_logEdit->clear();
    }
    appendLog(QStringLiteral("系统初始化完成。"));
}

MainWindow::~MainWindow()
{
    if (m_connectThread && m_connectThread->isRunning()) {
        m_connectThread->wait();
    }

    stopTimers();
    if (m_connected && m_socketFd != -1) {
        robot_arm_actions::disconnect(selectedRobotBrand(), m_socketFd);
    }
}

void MainWindow::setupStyleSheet()
{
    const QString style = QString::fromUtf8(R"(
        QMainWindow { background-color: #1e1e2e; }
        QWidget {
            color: #cdd6f4;
            font-family: "Microsoft YaHei", "SimHei", sans-serif;
            font-size: 12px;
        }
        QGroupBox {
            border: 1px solid #45475a;
            border-radius: 6px;
            margin-top: 12px;
            padding-top: 14px;
            font-weight: bold;
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
        }
        QTabBar::tab:selected {
            background-color: #45475a;
            color: #89b4fa;
            font-weight: bold;
        }
        QPushButton {
            background-color: #313244;
            color: #cdd6f4;
            border: 1px solid #45475a;
            border-radius: 5px;
            padding: 6px 16px;
            font-size: 24px;
            min-height: 56px;
        }
        QPushButton:hover {
            background-color: #45475a;
            border-color: #89b4fa;
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
        QPushButton[styleClass="success"]:disabled {
            background-color: #1e1e2e;
            border-color: #313244;
            color: #585b70;
        }
        QPushButton[styleClass="danger"] {
            background-color: #4e1e1e;
            border-color: #f38ba8;
            color: #f38ba8;
        }
        QPushButton[styleClass="danger"]:disabled {
            background-color: #1e1e2e;
            border-color: #313244;
            color: #585b70;
        }
        QPushButton[styleClass="warning"] {
            background-color: #4e3e1e;
            border-color: #f9e2af;
            color: #f9e2af;
        }
        QPushButton[styleClass="warning"]:disabled {
            background-color: #1e1e2e;
            border-color: #313244;
            color: #585b70;
        }
        QPushButton[styleClass="primary"] {
            background-color: #1e3a5f;
            border-color: #89b4fa;
            color: #89b4fa;
        }
        QPushButton[styleClass="primary"]:disabled {
            background-color: #1e1e2e;
            border-color: #313244;
            color: #585b70;
        }
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            background-color: #313244;
            color: #cdd6f4;
            border: 1px solid #45475a;
            border-radius: 4px;
            padding: 4px 8px;
            min-height: 24px;
        }
        QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
            border-color: #89b4fa;
        }
        QComboBox QAbstractItemView {
            background-color: #313244;
            color: #cdd6f4;
            border: 1px solid #45475a;
            selection-background-color: #45475a;
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
        QHeaderView::section {
            background-color: #313244;
            color: #89b4fa;
            padding: 4px;
            border: 1px solid #45475a;
            font-weight: bold;
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
            background-color: #11111b;
            border: 1px solid #313244;
            border-radius: 3px;
            min-height: 6px;
            max-height: 6px;
            text-align: center;
        }
        QProgressBar::chunk {
            background-color: #89b4fa;
            border-radius: 2px;
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
    topLayout->setAlignment(Qt::AlignTop);
    mainLayout->addLayout(topLayout);

    QSplitter *splitter = new QSplitter(Qt::Horizontal);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(0);

    QWidget *workpiecePanel = createWorkpieceSelectionPanel();
    workpiecePanel->setFixedWidth(kWorkpiecePageWidth);
    splitter->addWidget(workpiecePanel);

    QWidget *rightPanel = new QWidget();
    rightPanel->setFixedWidth(kLogPanelWidth);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(6);

    rightLayout->addWidget(createTcpPosePanel());
    if (m_workpieceSelection && m_workpieceSelection->selectionActionBar()) {
        QWidget *selectionActionBar = m_workpieceSelection->selectionActionBar();
        selectionActionBar->setParent(rightPanel);
        rightLayout->addWidget(selectionActionBar);
    }

    QGroupBox *logGroup = createStyledGroup(QStringLiteral("运行日志"));
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);
    m_logEdit = new QTextEdit();
    m_logEdit->setReadOnly(true);
    logLayout->addWidget(m_logEdit);
    rightLayout->addWidget(logGroup);

    splitter->addWidget(rightPanel);
    splitter->setCollapsible(0, false);
    splitter->setCollapsible(1, false);
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
    group->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    group->setMinimumHeight(188);
    group->setMaximumHeight(208);
    QGridLayout *layout = new QGridLayout(group);
    layout->setContentsMargins(10, 2, 10, 12);
    layout->setHorizontalSpacing(8);
    layout->setVerticalSpacing(12);

    layout->addWidget(new QLabel(QStringLiteral("控制器:")), 0, 0);
    m_connectionEndpointLabel = new QLabel(QStringLiteral("--"));
    m_connectionEndpointLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_connectionEndpointLabel, 0, 1);

    layout->addWidget(new QLabel(QStringLiteral("品牌:")), 0, 2);
    m_brandLabel = new QLabel(QStringLiteral("自动识别"));
    layout->addWidget(m_brandLabel, 0, 3);

    m_connectBtn = createStyledButton(QStringLiteral("连接"), QStringLiteral("success"));
    m_servoPowerOnBtn = createStyledButton(QStringLiteral("使能"), QStringLiteral("success"));
    m_disconnectBtn = createStyledButton(QStringLiteral("断开"), QStringLiteral("danger"));
    const int connectionButtonWidth = 88;
    for (QPushButton *button : {m_connectBtn, m_servoPowerOnBtn, m_disconnectBtn}) {
        button->setFixedWidth(connectionButtonWidth);
        button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
    m_servoPowerOnBtn->setEnabled(false);
    m_disconnectBtn->setEnabled(false);
    layout->addWidget(m_connectBtn, 0, 5, Qt::AlignTop);
    layout->addWidget(m_servoPowerOnBtn, 0, 6, Qt::AlignTop);
    layout->addWidget(m_disconnectBtn, 0, 7, Qt::AlignTop);

    m_connStatusLabel = new QLabel(QStringLiteral("● 未连接"));
    m_connStatusLabel->setProperty("styleClass", "status-disconnected");
    layout->addWidget(m_connStatusLabel, 0, 4);

    layout->addWidget(new QLabel(QStringLiteral("工件 ID:")), 1, 0);
    m_workpieceIdEdit = new QLineEdit();
    m_workpieceIdEdit->setReadOnly(true);
    m_workpieceIdEdit->setPlaceholderText(QStringLiteral("未选择"));
    m_workpieceIdEdit->setMinimumWidth(140);
    layout->addWidget(m_workpieceIdEdit, 1, 1);

    layout->addWidget(new QLabel(QStringLiteral("作业名称:")), 1, 2);
    m_jobNameEdit = new QLineEdit(QStringLiteral("TEST"));
    m_jobNameEdit->setMinimumWidth(140);
    layout->addWidget(m_jobNameEdit, 1, 3, 1, 5);

    m_connProgress = new QProgressBar();
    m_connProgress->setRange(0, 100);
    m_connProgress->setValue(0);
    m_connProgress->setTextVisible(false);
    m_connProgress->setVisible(true);
    layout->addWidget(m_connProgress, 2, 0, 1, 8);

    layout->setColumnStretch(1, 1);
    layout->setColumnStretch(3, 1);

    connect(m_connectBtn, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(m_servoPowerOnBtn, &QPushButton::clicked, this, &MainWindow::onServoPowerOn);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);

    return group;
}

QWidget* MainWindow::createServoPanel()
{
    QGroupBox *group = createStyledGroup(QStringLiteral("作业控制"));
    group->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    QGridLayout *layout = new QGridLayout(group);
    layout->setSpacing(8);

    m_topJobStartBtn = createStyledButton(QStringLiteral("开始"), QStringLiteral("success"));
    m_topJobPauseContinueBtn = createStyledButton(QStringLiteral("暂停"), QStringLiteral("warning"));
    m_topJobStopBtn = createStyledButton(QStringLiteral("停止"), QStringLiteral("danger"));
    m_topJobResetBtn = createStyledButton(QStringLiteral("复位"), QStringLiteral("primary"));
    const int jobButtonWidth = 120;
    for (QPushButton *button : {m_topJobStartBtn, m_topJobPauseContinueBtn, m_topJobStopBtn,
                                m_topJobResetBtn}) {
        button->setMinimumWidth(jobButtonWidth);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setEnabled(false);
    }
    layout->addWidget(m_topJobStartBtn, 0, 0, 1, 2);
    layout->addWidget(m_topJobPauseContinueBtn, 0, 2, 1, 2);
    layout->addWidget(m_topJobStopBtn, 0, 4, 1, 2);
    layout->addWidget(m_topJobResetBtn, 0, 6, 1, 2);

    for (int column = 0; column < 8; ++column) {
        layout->setColumnStretch(column, 1);
    }

    m_topJobStatusLabel = new QLabel(QStringLiteral("状态: 停止"));
    m_topJobStatusLabel->setProperty("styleClass", "subtle");
    layout->addWidget(m_topJobStatusLabel, 1, 0, 1, 8);

    connect(m_topJobStartBtn, &QPushButton::clicked, this, &MainWindow::onTopJobStart);
    connect(m_topJobPauseContinueBtn, &QPushButton::clicked, this, &MainWindow::onTopJobPauseContinue);
    connect(m_topJobStopBtn, &QPushButton::clicked, this, &MainWindow::onTopJobStop);
    connect(m_topJobResetBtn, &QPushButton::clicked, this, &MainWindow::onTopJobReset);

    return group;
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
    m_doPortSpin->setValue(kWindGrinderPort);
    doSetLayout->addWidget(m_doPortSpin);

    doSetLayout->addWidget(new QLabel(QStringLiteral("值:")));
    m_doValueCombo = new QComboBox();
    m_doValueCombo->addItems({QStringLiteral("OFF (0)"), QStringLiteral("ON (1)")});
    doSetLayout->addWidget(m_doValueCombo);

    m_doSetBtn = createStyledButton(QStringLiteral("设置输出"), QStringLiteral("primary"));
    m_ioRefreshBtn = createStyledButton(QStringLiteral("刷新 IO"), QStringLiteral("warning"));
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
    m_diTable->setHorizontalHeaderLabels({QStringLiteral("端口"), QStringLiteral("状态"),
                                          QStringLiteral("端口"), QStringLiteral("状态")});
    m_diTable->horizontalHeader()->setStretchLastSection(true);
    m_diTable->verticalHeader()->setVisible(false);
    m_diTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
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
    m_doTable->setHorizontalHeaderLabels({QStringLiteral("端口"), QStringLiteral("状态"),
                                          QStringLiteral("端口"), QStringLiteral("状态")});
    m_doTable->horizontalHeader()->setStretchLastSection(true);
    m_doTable->verticalHeader()->setVisible(false);
    m_doTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
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

    QGroupBox *editGroup = createStyledGroup(QStringLiteral("全局变量"));
    QGridLayout *editLayout = new QGridLayout(editGroup);
    editLayout->addWidget(new QLabel(QStringLiteral("变量名:")), 0, 0);
    m_globalVarNameEdit = new QLineEdit(QStringLiteral("GB009"));
    editLayout->addWidget(m_globalVarNameEdit, 0, 1);

    editLayout->addWidget(new QLabel(QStringLiteral("预设:")), 1, 0);
    m_globalVarPresetCombo = new QComboBox();
    m_globalVarPresetCombo->addItem(QStringLiteral("GB009 - 打磨开始信号"), QStringLiteral("GB009"));
    m_globalVarPresetCombo->addItem(QStringLiteral("GB010 - 打磨暂停信号"), QStringLiteral("GB010"));
    m_globalVarPresetCombo->addItem(QStringLiteral("GI000 - 整数变量示例"), QStringLiteral("GI000"));
    m_globalVarPresetCombo->addItem(QStringLiteral("GD000 - 浮点变量示例"), QStringLiteral("GD000"));
    QFile presetFile(globalVarPresetPath());
    if (presetFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_globalVarPresetCombo->clear();
        QTextStream in(&presetFile);
        const QRegularExpression variablePattern(QStringLiteral("\\bG[BDI]\\d+\\b"),
                                                 QRegularExpression::CaseInsensitiveOption);
        while (!in.atEnd()) {
            const QString line = in.readLine().trimmed();
            const QRegularExpressionMatch match = variablePattern.match(line);
            if (!match.hasMatch()) {
                continue;
            }
            const QString varName = match.captured(0).toUpper();
            QString displayName = line.simplified();
            if (displayName.size() > 64) {
                displayName = displayName.left(61) + QStringLiteral("...");
            }
            m_globalVarPresetCombo->addItem(QStringLiteral("%1 - %2").arg(varName, displayName), varName);
        }
    }
    editLayout->addWidget(m_globalVarPresetCombo, 1, 1);

    editLayout->addWidget(new QLabel(QStringLiteral("值:")), 2, 0);
    m_globalVarValueSpin = new QDoubleSpinBox();
    m_globalVarValueSpin->setRange(-9999999.0, 9999999.0);
    m_globalVarValueSpin->setDecimals(3);
    editLayout->addWidget(m_globalVarValueSpin, 2, 1);

    m_globalVarTypeLabel = new QLabel(QStringLiteral("类型: B"));
    m_globalVarModeLabel = new QLabel(QStringLiteral("连接后可读写"));
    m_globalVarResultLabel = new QLabel(QStringLiteral("结果: --"));
    editLayout->addWidget(m_globalVarTypeLabel, 3, 0);
    editLayout->addWidget(m_globalVarModeLabel, 3, 1);
    editLayout->addWidget(m_globalVarResultLabel, 4, 0, 1, 2);

    m_globalVarSetBtn = createStyledButton(QStringLiteral("写入变量"), QStringLiteral("primary"));
    m_globalVarGetBtn = createStyledButton(QStringLiteral("读取变量"), QStringLiteral("warning"));
    m_windGrinderBtn = createStyledButton(QStringLiteral("启动风磨"), QStringLiteral("success"));
    m_globalVarSetBtn->setEnabled(false);
    m_globalVarGetBtn->setEnabled(false);
    editLayout->addWidget(m_globalVarSetBtn, 5, 0);
    editLayout->addWidget(m_globalVarGetBtn, 5, 1);
    editLayout->addWidget(m_windGrinderBtn, 6, 0, 1, 2);

    QLabel *huichuanApiLabel = new QLabel(
        QStringLiteral("汇川全局变量实际调用: GB/B -> IMC100_Get_B / IMC100_Set_B；"
                       "GI/R -> IMC100_Get_R / IMC100_Set_R；"
                       "GD/D -> IMC100_Get_D / IMC100_Set_D。"
                       "变量名中的数字作为 SDK 编号，例如 GB009 使用编号 9。"));
    huichuanApiLabel->setWordWrap(true);
    huichuanApiLabel->setProperty("styleClass", "subtle");
    editLayout->addWidget(huichuanApiLabel, 7, 0, 1, 2);

    connect(m_globalVarPresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) {
                const QString name = m_globalVarPresetCombo->itemData(index).toString();
                if (!name.isEmpty()) {
                    m_globalVarNameEdit->setText(name);
                }
            });
    connect(m_globalVarNameEdit, &QLineEdit::textChanged, this, &MainWindow::applyGlobalVariantInputType);
    connect(m_globalVarSetBtn, &QPushButton::clicked, this, &MainWindow::onSetGlobalVariant);
    connect(m_globalVarGetBtn, &QPushButton::clicked, this, &MainWindow::onGetGlobalVariant);
    connect(m_windGrinderBtn, &QPushButton::clicked, this, &MainWindow::onToggleWindGrinder);

    mainLayout->addWidget(editGroup);

    QGroupBox *ioGroup = createStyledGroup(QStringLiteral("全局 IO"));
    QVBoxLayout *ioLayout = new QVBoxLayout(ioGroup);
    ioLayout->setContentsMargins(0, 0, 0, 0);
    ioLayout->addWidget(createIOPanel());
    mainLayout->addWidget(ioGroup);

    mainLayout->addStretch();
    applyGlobalVariantInputType();
    return panel;
}

QWidget* MainWindow::createTcpPosePanel()
{
    QGroupBox *group = createStyledGroup(QStringLiteral("末端位置姿态"));
    QGridLayout *layout = new QGridLayout(group);
    layout->setContentsMargins(10, 14, 10, 10);
    layout->setHorizontalSpacing(10);
    layout->setVerticalSpacing(6);

    const QStringList names = {
        QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z"),
        QStringLiteral("Rx"), QStringLiteral("Ry"), QStringLiteral("Rz")
    };

    for (int i = 0; i < names.size(); ++i) {
        const int row = i / 3;
        const int column = (i % 3) * 2;

        QLabel *nameLabel = new QLabel(names.at(i) + QStringLiteral(":"));
        nameLabel->setProperty("styleClass", "subtle");
        layout->addWidget(nameLabel, row, column);

        QLabel *valueLabel = new QLabel(QStringLiteral("--"));
        valueLabel->setMinimumWidth(72);
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_tcpPoseValueLabels[i] = valueLabel;
        layout->addWidget(valueLabel, row, column + 1);
    }

    QLabel *unitLabel = new QLabel(QStringLiteral("位置单位 mm，姿态单位 deg"));
    unitLabel->setProperty("styleClass", "subtle");
    layout->addWidget(unitLabel, 2, 0, 1, 6);

    return group;
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

void MainWindow::applyGlobalVariantInputType()
{
    const QString name = m_globalVarNameEdit ? m_globalVarNameEdit->text().trimmed().toUpper() : QString();
    if (!m_globalVarValueSpin || !m_globalVarTypeLabel) {
        return;
    }

    if (name.startsWith(QStringLiteral("GB")) || name.startsWith(QStringLiteral("B"))) {
        m_globalVarValueSpin->setDecimals(0);
        m_globalVarValueSpin->setRange(0, 1);
        m_globalVarTypeLabel->setText(QStringLiteral("类型: B"));
    } else if (name.startsWith(QStringLiteral("GI")) || name.startsWith(QStringLiteral("R"))) {
        m_globalVarValueSpin->setDecimals(0);
        m_globalVarValueSpin->setRange(-2147483647.0, 2147483647.0);
        m_globalVarTypeLabel->setText(QStringLiteral("类型: R"));
    } else {
        m_globalVarValueSpin->setDecimals(3);
        m_globalVarValueSpin->setRange(-9999999.0, 9999999.0);
        m_globalVarTypeLabel->setText(QStringLiteral("类型: D"));
    }
}

void MainWindow::refreshGlobalVariantModeLabel()
{
    if (!m_globalVarModeLabel) {
        return;
    }
    if (!m_connected) {
        m_globalVarModeLabel->setText(QStringLiteral("未连接"));
    } else {
        m_globalVarModeLabel->setText(QStringLiteral("已连接"));
    }
}

bool MainWindow::currentGlobalVariantName(QString& nameOut) const
{
    nameOut = m_globalVarNameEdit ? m_globalVarNameEdit->text().trimmed().toUpper() : QString();
    static const QRegularExpression variablePattern(QStringLiteral("^G?[BRID]\\d+$"),
                                                    QRegularExpression::CaseInsensitiveOption);
    return variablePattern.match(nameOut).hasMatch();
}

void MainWindow::loadConnectionSettings()
{
    QString ip;
    QString port;
    m_lastSuccessfulBrand = readLastSuccessfulBrand();
    QString brand = m_lastSuccessfulBrand == QStringLiteral("huichuan")
        ? QStringLiteral("huichuan")
        : QStringLiteral("yinglian");

    if (!readConnectionSettingsForBrand(brand, ip, port)) {
        const QString fallbackBrand = brand == QStringLiteral("yinglian")
            ? QStringLiteral("huichuan")
            : QStringLiteral("yinglian");
        if (readConnectionSettingsForBrand(fallbackBrand, ip, port)) {
            brand = fallbackBrand;
        }
    }

    m_robotIp = ip;
    m_robotPort = port;
    m_robotBrand = brand;
    applySelectedRobotBrand();

    if (m_brandLabel) {
        m_brandLabel->setText(QStringLiteral("自动识别"));
    }
    if (m_connectionEndpointLabel) {
        m_connectionEndpointLabel->setText(connectionSummaryText(QStringLiteral("自动识别"), m_robotIp, m_robotPort));
    }
}

void MainWindow::saveConnectionSettings() const
{
    return;
}

QString MainWindow::connectionBrandText() const
{
    return brandTextForRobotBrand(selectedRobotBrand());
}

void MainWindow::applySelectedRobotBrand()
{
    set_robot_brand(selectedRobotBrand());
}

RobotBrand MainWindow::selectedRobotBrand() const
{
    return m_robotBrand == QStringLiteral("yinglian") ? RobotBrand::Yinglian : RobotBrand::Huichuan;
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

    QFile logFile(logFilePathForToday());
    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
        const QString plainText = QStringLiteral("[%1] %2\n")
                                      .arg(timestamp, msg)
                                      .remove(QRegularExpression(QStringLiteral("<[^>]*>")));
        QTextStream out(&logFile);
        out << plainText;
    }
}

void MainWindow::stopTimers()
{
    if (m_updateTimer && m_updateTimer->isActive()) {
        m_updateTimer->stop();
    }
}

void MainWindow::setConnectionUi(bool connected)
{
    m_connectBtn->setEnabled(!connected && !m_connecting);
    if (m_servoPowerOnBtn) {
        m_servoPowerOnBtn->setEnabled(connected && !m_connecting);
    }
    if (m_globalVarSetBtn) {
        m_globalVarSetBtn->setEnabled(connected && !m_connecting);
    }
    if (m_globalVarGetBtn) {
        m_globalVarGetBtn->setEnabled(connected && !m_connecting);
    }
    if (m_doSetBtn) {
        m_doSetBtn->setEnabled(connected && !m_connecting);
    }
    if (m_ioRefreshBtn) {
        m_ioRefreshBtn->setEnabled(connected && !m_connecting);
    }
    m_disconnectBtn->setEnabled(connected && !m_connecting);
    updateTopJobButtons();
}

void MainWindow::updateTopJobButtons()
{
    const bool enabled = m_connected && !m_connecting && m_workpieceSelected;
    if (m_topJobStartBtn) {
        m_topJobStartBtn->setEnabled(enabled);
    }
    if (m_topJobPauseContinueBtn) {
        m_topJobPauseContinueBtn->setEnabled(enabled);
    }
    if (m_topJobStopBtn) {
        m_topJobStopBtn->setEnabled(enabled);
    }
    if (m_topJobResetBtn) {
        m_topJobResetBtn->setEnabled(enabled);
    }
}

void MainWindow::setConnectionInProgress(bool inProgress)
{
    m_connecting = inProgress;
    if (m_connProgress) {
        if (inProgress) {
            m_connProgress->setRange(0, 0);
        } else {
            m_connProgress->setRange(0, 100);
            m_connProgress->setValue(0);
        }
        m_connProgress->setVisible(true);
    }
    setConnectionUi(m_connected);
}

void MainWindow::finishConnect(const QString& ip, const QString& port, SOCKETFD socketFd, qint64 elapsedMs,
                               const QString& failureDetail, const QString& tcpProbeDetail,
                               const QString& detectedBrand)
{
    setConnectionInProgress(false);
    appendLog(QStringLiteral("连接调用返回: socket=%1, 耗时=%2 ms").arg(socketFd).arg(elapsedMs));

    if (socketFd == -1) {
        appendLog(QStringLiteral("<span style='color:#f38ba8;'>连接控制器 %1:%2 失败。</span>").arg(ip, port));
        if (!failureDetail.isEmpty()) {
            appendLog(QStringLiteral("<span style='color:#f38ba8;'>连接失败原因: %1</span>").arg(failureDetail));
        } else {
            appendLog(QStringLiteral("<span style='color:#f38ba8;'>连接失败原因: SDK 未返回更详细的失败信息。</span>"));
        }
        if (!tcpProbeDetail.isEmpty()) {
            appendLog(QStringLiteral("<span style='color:#f9e2af;'>%1</span>").arg(tcpProbeDetail));
        }
        QMessageBox::warning(
            this,
            QStringLiteral("连接失败"),
            QStringLiteral("无法连接到控制器 %1:%2。%3%4")
                .arg(ip, port)
                .arg(failureDetail.isEmpty() ? QString() : QStringLiteral("\n\n原因: %1").arg(failureDetail))
                .arg(tcpProbeDetail.isEmpty() ? QString() : QStringLiteral("\n\n%1").arg(tcpProbeDetail)));
        m_connected = false;
        m_socketFd = -1;
        m_pollFailCount = 0;
        setConnectionUi(false);
        updateConnectionStatusLabel(QStringLiteral("● 未连接"), false);
        refreshGlobalVariantModeLabel();
        return;
    }

    m_socketFd = socketFd;
    m_connected = true;
    m_pollFailCount = 0;
    m_pollSequence = 0;
    m_robotBrand = detectedBrand == QStringLiteral("yinglian") ? QStringLiteral("yinglian") : QStringLiteral("huichuan");
    applySelectedRobotBrand();
    saveLastSuccessfulBrand(m_robotBrand);
    m_lastSuccessfulBrand = m_robotBrand;
    setConnectionUi(true);
    if (m_brandLabel) {
        m_brandLabel->setText(connectionBrandText());
    }
    updateConnectionStatusLabel(QStringLiteral("● 已连接 %1 %2:%3").arg(connectionBrandText(), ip, port), true);
    if (selectedRobotBrand() == RobotBrand::Huichuan) {
        const QString loginDetail = QString::fromStdString(robot_login_admin_user()).trimmed();
        if (!loginDetail.isEmpty()) {
            appendLog(QStringLiteral("<span style='color:#f9e2af;'>%1</span>").arg(loginDetail));
        }
    }
    pollRobotState();
    refreshStatusSummary();
    refreshGlobalVariantModeLabel();
    m_updateTimer->start();
    appendLog(QStringLiteral("<span style='color:#a6e3a1;'>已成功连接到%1控制器 %2:%3</span>")
                  .arg(connectionBrandText(), ip, port));
}

void MainWindow::updateConnectionStatusLabel(const QString& text, bool connectedStyle)
{
    m_connStatusLabel->setText(text);
    m_connStatusLabel->setProperty("styleClass", connectedStyle ? "status-connected" : "status-disconnected");
    m_connStatusLabel->style()->unpolish(m_connStatusLabel);
    m_connStatusLabel->style()->polish(m_connStatusLabel);
}

void MainWindow::refreshStatusSummary()
{
    if (!m_connected) {
        m_tcpPose.clear();
        refreshTcpPoseLabels();
        if (m_servoStatusLabel) {
            m_servoStatusLabel->setText(QStringLiteral("伺服状态: --"));
        }
        if (m_robotRunStateLabel) {
            m_robotRunStateLabel->setText(QStringLiteral("运行状态: --"));
        }
        if (m_robotTypeLabel) {
            m_robotTypeLabel->setText(QStringLiteral("机器人类型: --"));
        }
        return;
    }

    if (m_servoStatusLabel) {
        m_servoStatusLabel->setText(QStringLiteral("伺服状态: %1").arg(servoStateText(m_servoState)));
    }
    if (m_robotRunStateLabel) {
        m_robotRunStateLabel->setText(QStringLiteral("运行状态: %1").arg(runStateText(m_runState)));
    }
    if (m_robotTypeLabel) {
        m_robotTypeLabel->setText(QStringLiteral("机器人类型: %1").arg(robotTypeName(m_robotType)));
    }
    refreshTcpPoseLabels();
}

void MainWindow::refreshTcpPoseLabels()
{
    for (int i = 0; i < 6; ++i) {
        if (!m_tcpPoseValueLabels[i]) {
            continue;
        }

        if (m_tcpPose.size() > static_cast<size_t>(i)) {
            m_tcpPoseValueLabels[i]->setText(QString::number(m_tcpPose.at(i), 'f', 3));
        } else {
            m_tcpPoseValueLabels[i]->setText(QStringLiteral("--"));
        }
    }
}

void MainWindow::setTopJobStatus(const QString& status, bool paused)
{
    m_topJobPaused = paused;
    if (m_topJobPauseContinueBtn) {
        m_topJobPauseContinueBtn->setText(paused ? QStringLiteral("继续") : QStringLiteral("暂停"));
    }
    if (m_topJobStatusLabel) {
        m_topJobStatusLabel->setText(QStringLiteral("状态: %1").arg(status));
    }
}

bool MainWindow::ensureJobControllerReady(const QString& actionName)
{
    if (!m_connected) {
        appendLog(QStringLiteral("<span style='color:#f9e2af;'>%1失败：请先连接控制器。</span>").arg(actionName));
        return false;
    }
    if (m_socketFd == -1) {
        appendLog(QStringLiteral("<span style='color:#f9e2af;'>%1失败：控制器连接无效。</span>").arg(actionName));
        return false;
    }
    return true;
}

bool MainWindow::pollRobotState()
{
    if (!m_connected || m_socketFd == -1) {
        return false;
    }

    const int pollSequence = ++m_pollSequence;
    int failCount = 0;

    int servoState = 0;
    const Result servoResult = robot_get_servo_state(m_socketFd, servoState);
    if (servoResult == SUCCESS) {
        m_servoState = servoState;
    } else {
        ++failCount;
    }

    int runState = 0;
    const Result runResult = robot_get_robot_running_state(m_socketFd, runState);
    if (runResult == SUCCESS) {
        m_runState = runState;
    } else {
        ++failCount;
    }

    int robotType = 0;
    const Result typeResult = robot_get_robot_type(m_socketFd, robotType);
    if (typeResult == SUCCESS) {
        m_robotType = robotType;
    } else {
        ++failCount;
    }

    std::vector<double> tcpPose;
    const Result poseResult = robot_get_current_position(m_socketFd, 1, tcpPose);
    const bool poseValid = poseResult == SUCCESS && tcpPose.size() >= 6;
    if (poseValid) {
        m_tcpPose.assign(tcpPose.begin(), tcpPose.begin() + 6);
    } else {
        m_tcpPose.clear();
    }

    // 三个查询全部失败，连接可能已断开
    if (failCount >= 3) {
        ++m_pollFailCount;
    } else {
        m_pollFailCount = 0;
    }

    if (failCount > 0 || !poseValid || m_pollFailCount > 0) {
        appendLog(QStringLiteral(
                      "<span style='color:#f9e2af;'>轮询#%1: servo=%2, run=%3, type=%4, pose=%5(size=%6), "
                      "状态失败%7/3, 连续全失败%8/3</span>")
                      .arg(pollSequence)
                      .arg(resultToString(servoResult))
                      .arg(resultToString(runResult))
                      .arg(resultToString(typeResult))
                      .arg(resultToString(poseResult))
                      .arg(tcpPose.size())
                      .arg(failCount)
                      .arg(m_pollFailCount));
    }

    if (m_pollFailCount >= 3) {
        appendLog(QStringLiteral(
                      "<span style='color:#f38ba8;'>断开判定: 连续 3 次轮询中 servo/run/type 三个状态接口全部失败。</span>"));
    }

    return m_pollFailCount < 3;
}

QString MainWindow::normalizedJobName() const
{
    QString jobName = m_jobNameEdit ? m_jobNameEdit->text().trimmed() : QString();
    if (jobName.endsWith(QStringLiteral(".JBR"), Qt::CaseInsensitive)) {
        jobName.chop(4);
    }
    return jobName;
}

QString MainWindow::resultToString(Result result) const
{
    switch (result) {
    case SUCCESS: return QStringLiteral("SUCCESS");
    case RECEIVE_FAILED: return QStringLiteral("RECEIVE_FAILED");
    case DISCONNECT: return QStringLiteral("DISCONNECT");
    case PARAM_ERR: return QStringLiteral("PARAM_ERR");
    case OPERATION_NOT_ALLOWED: return QStringLiteral("OPERATION_NOT_ALLOWED");
    case EXCEPTION: return QStringLiteral("EXCEPTION");
    case TIMEOUT: return QStringLiteral("TIMEOUT");
    default: return QStringLiteral("UNKNOWN");
    }
}

QString MainWindow::robotTypeName(int type) const
{
    switch (type) {
    case 1: return QStringLiteral("六轴串联");
    case 2: return QStringLiteral("四轴 SCARA");
    case 4: return QStringLiteral("四轴串联");
    case 5: return QStringLiteral("单轴");
    case 6: return QStringLiteral("五轴串联");
    case 7: return QStringLiteral("六轴协作");
    case 8: return QStringLiteral("二轴 SCARA");
    case 9: return QStringLiteral("三轴 SCARA");
    case 12: return QStringLiteral("七轴串联");
    default: return QStringLiteral("未知类型(%1)").arg(type);
    }
}

void MainWindow::onConnectClicked()
{
    if (m_connecting) {
        return;
    }

    const QString lastBrand = readLastSuccessfulBrand();
    QString yinglianIp;
    QString yinglianPort;
    const bool hasYinglian = readConnectionSettingsForBrand(QStringLiteral("yinglian"), yinglianIp, yinglianPort);
    QString huichuanIp;
    QString huichuanPort;
    const bool hasHuichuan = readConnectionSettingsForBrand(QStringLiteral("huichuan"), huichuanIp, huichuanPort);
    if (!hasHuichuan && !hasYinglian) {
        QMessageBox::warning(
            this,
            QStringLiteral("配置错误"),
            QStringLiteral("未找到可用连接配置。请确认 robot_connection_huichuan.ini 或 robot_connection_yinglian.ini 中存在 connection/ip 和 connection/port。"));
        return;
    }

    struct Candidate {
        RobotBrand brand;
        QString ip;
        QString port;
        bool hasConfig = false;
    };

    const bool historyIsHuichuan = lastBrand == QStringLiteral("huichuan");
    const Candidate firstCandidate = historyIsHuichuan
        ? Candidate{RobotBrand::Huichuan, huichuanIp, huichuanPort, hasHuichuan}
        : Candidate{RobotBrand::Yinglian, yinglianIp, yinglianPort, hasYinglian};
    const Candidate secondCandidate = historyIsHuichuan
        ? Candidate{RobotBrand::Yinglian, yinglianIp, yinglianPort, hasYinglian}
        : Candidate{RobotBrand::Huichuan, huichuanIp, huichuanPort, hasHuichuan};
    const Candidate displayCandidate = firstCandidate.hasConfig ? firstCandidate : secondCandidate;

    m_robotIp = displayCandidate.ip;
    m_robotPort = displayCandidate.port;
    m_robotBrand = displayCandidate.brand == RobotBrand::Yinglian ? QStringLiteral("yinglian") : QStringLiteral("huichuan");
    applySelectedRobotBrand();
    if (m_brandLabel) {
        m_brandLabel->setText(QStringLiteral("自动识别"));
    }
    if (m_connectionEndpointLabel) {
        m_connectionEndpointLabel->setText(connectionSummaryText(QStringLiteral("自动识别"), m_robotIp, m_robotPort));
    }

    appendLog(QStringLiteral("连接历史品牌: %1").arg(lastBrand.isEmpty() ? QStringLiteral("未记录") : lastBrand));
    appendLog(QStringLiteral("正在从配置文件自动识别并连接控制器 %1:%2 ...").arg(m_robotIp, m_robotPort));
    updateConnectionStatusLabel(QStringLiteral("● 正在自动识别 %1:%2").arg(m_robotIp, m_robotPort), false);
    setConnectionInProgress(true);

    QThread *thread = QThread::create([this, firstCandidate, secondCandidate]() {
        struct ConnectAttempt {
            SOCKETFD socketFd = -1;
            qint64 elapsedMs = 0;
            QString failureDetail;
        };

        auto tryBrand = [&](RobotBrand brand, const QString& ip, const QString& port) {
            ConnectAttempt attempt;
            set_robot_brand(brand);
            const QDateTime connectStartTime = QDateTime::currentDateTime();
            attempt.socketFd = robot_arm_actions::connect(brand, ip.toStdString(), port.toStdString());
            attempt.elapsedMs = connectStartTime.msecsTo(QDateTime::currentDateTime());
            if (attempt.socketFd == -1 && brand == RobotBrand::Huichuan) {
                attempt.failureDetail = QString::fromStdString(robot_arm_actions::last_connect_error(brand));
            }
            return attempt;
        };

        auto tryCandidate = [&](const Candidate& candidate) -> bool {
            if (!candidate.hasConfig) {
                return false;
            }
            const ConnectAttempt attempt = tryBrand(candidate.brand, candidate.ip, candidate.port);
            if (attempt.socketFd == -1) {
                return false;
            }
            QMetaObject::invokeMethod(this, [this, attempt, candidate]() {
                finishConnect(candidate.ip, candidate.port, attempt.socketFd, attempt.elapsedMs, QString(), QString(),
                              candidate.brand == RobotBrand::Yinglian ? QStringLiteral("yinglian") : QStringLiteral("huichuan"));
            }, Qt::QueuedConnection);
            return true;
        };

        const bool connected = tryCandidate(firstCandidate) || tryCandidate(secondCandidate);

        if (connected) {
            return;
        }

        const QString failureIp = firstCandidate.ip;
        const QString failurePort = firstCandidate.port;
        const QString detectedBrand = firstCandidate.brand == RobotBrand::Yinglian
            ? QStringLiteral("yinglian")
            : QStringLiteral("huichuan");
        QStringList failureReasons;
        if (firstCandidate.hasConfig) {
            failureReasons << (firstCandidate.brand == RobotBrand::Huichuan
                                   ? QStringLiteral("汇川配置连接失败。")
                                   : QStringLiteral("盈联配置连接失败。"));
        }
        if (secondCandidate.hasConfig) {
            failureReasons << (secondCandidate.brand == RobotBrand::Huichuan
                                   ? QStringLiteral("汇川配置连接失败。")
                                   : QStringLiteral("盈联配置连接失败。"));
        }
        const QString failureDetail = failureReasons.join(QStringLiteral(" "));
        const QString tcpProbeDetail = probeTcpEndpoint(failureIp, failurePort);
        QMetaObject::invokeMethod(this, [this, failureIp, failurePort, failureDetail, tcpProbeDetail, detectedBrand]() {
            finishConnect(failureIp, failurePort, -1, 0, failureDetail, tcpProbeDetail, detectedBrand);
        }, Qt::QueuedConnection);
    });
    thread->setParent(this);
    m_connectThread = thread;
    connect(thread, &QThread::finished, this, [this, thread]() {
        if (m_connectThread == thread) {
            m_connectThread = nullptr;
        }
        thread->deleteLater();
    });
    thread->start();
}

void MainWindow::onDisconnectClicked()
{
    if (m_socketFd != -1) {
        robot_arm_actions::disconnect(selectedRobotBrand(), m_socketFd);
    }
    stopTimers();
    m_connected = false;
    m_socketFd = -1;
    setConnectionUi(false);
    updateConnectionStatusLabel(QStringLiteral("● 未连接"), false);
    if (m_brandLabel) {
        m_brandLabel->setText(QStringLiteral("自动识别"));
    }
    refreshStatusSummary();
    refreshGlobalVariantModeLabel();
    appendLog(QStringLiteral("<span style='color:#f38ba8;'>已断开与控制器的连接。</span>"));
}

void MainWindow::onServoReady()
{
    if (!m_connected) return;
    const Result result = robot_set_servo_state(m_socketFd, 1);
    appendLog(QStringLiteral("设置伺服就绪 -> %1").arg(resultToString(result)));
}

void MainWindow::onServoPowerOn()
{
    if (!m_connected) return;
    const Result result = robot_arm_actions::enable(selectedRobotBrand(), m_socketFd);
    appendLog(QStringLiteral("伺服上电 -> %1").arg(resultToString(result)));
}

void MainWindow::onServoPowerOff()
{
    if (!m_connected) return;
    const Result result = robot_set_servo_poweroff(m_socketFd);
    appendLog(QStringLiteral("伺服下电 -> %1").arg(resultToString(result)));
}

void MainWindow::onClearError()
{
    if (!m_connected) return;
    const Result result = robot_clear_error(m_socketFd);
    appendLog(QStringLiteral("清除报警 -> %1").arg(resultToString(result)));
}

void MainWindow::onModeChanged(int index)
{
    const QStringList modeNames = {QStringLiteral("示教"), QStringLiteral("远程"), QStringLiteral("运行")};
    appendLog(QStringLiteral("切换模式: %1").arg(modeNames.value(index)));
    if (m_connected) {
        const Result result = robot_set_current_mode(m_socketFd, index);
        if (result != SUCCESS) {
            appendLog(QStringLiteral("<span style='color:#f38ba8;'>模式切换失败: %1</span>").arg(resultToString(result)));
        }
    }
}

void MainWindow::onTopJobStart()
{
    if (!ensureJobControllerReady(QStringLiteral("开始作业"))) return;

    QString name = normalizedJobName();
    if (selectedRobotBrand() == RobotBrand::Huichuan) {
        appendLog(QStringLiteral("<span style='color:#f9e2af;'>汇川开始作业: 正在读取示教器当前激活程序...</span>"));
        std::string currentPath;
        const Result pathResult = queryCurrentJobProgram(m_socketFd, currentPath);
        if (pathResult != SUCCESS) {
            appendLog(QStringLiteral("<span style='color:#f38ba8;'>汇川启动失败：未能获取示教器当前激活程序 -> %1</span>")
                          .arg(resultToString(pathResult)));
            return;
        }
        name = QString::fromLocal8Bit(currentPath.data(), static_cast<int>(currentPath.size()));
        const QString configuredProgram = huichuanConfiguredActiveProgramName();
        if (name.trimmed().isEmpty()) {
            appendLog(QStringLiteral("<span style='color:#f38ba8;'>汇川启动失败：SDK 返回 SUCCESS，但示教器当前激活程序名为空；配置文件程序名 = %1。</span>")
                          .arg(programNameWithoutFileType(configuredProgram).isEmpty()
                                   ? QStringLiteral("--")
                                   : programNameWithoutFileType(configuredProgram)));
            return;
        }
        appendLog(QStringLiteral("<span style='color:#a6e3a1;'>汇川开始作业: 当前激活程序 = %1</span>").arg(name));

        const QString currentProgramName = programNameWithoutFileType(name);
        const QString configuredProgramName = programNameWithoutFileType(configuredProgram);
        appendLog(QStringLiteral("<span style='color:#f9e2af;'>汇川开始作业: 示教器当前激活程序名 = %1，配置文件程序名 = %2</span>")
                      .arg(currentProgramName.isEmpty() ? QStringLiteral("--") : currentProgramName,
                           configuredProgramName.isEmpty() ? QStringLiteral("--") : configuredProgramName));
        if (configuredProgramName.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("配置错误"),
                                 QStringLiteral("请在 %1 中配置 active_program/name。")
                                     .arg(connectionConfigPathForBrand(QStringLiteral("huichuan"))));
            appendLog(QStringLiteral("<span style='color:#f38ba8;'>汇川启动失败：配置文件未指定 active_program/name。</span>"));
            return;
        }
        if (currentProgramName.compare(configuredProgramName, Qt::CaseInsensitive) != 0) {
            QMessageBox::warning(
                this,
                QStringLiteral("程序不匹配"),
                QStringLiteral("当前示教器激活程序为 \"%1\"，配置文件程序为 \"%2\"，请确认后再开始。")
                    .arg(currentProgramName, configuredProgramName));
            appendLog(QStringLiteral("<span style='color:#f38ba8;'>汇川启动失败：当前激活程序 \"%1\" 与配置程序 \"%2\" 不一致。</span>")
                          .arg(currentProgramName, configuredProgramName));
            return;
        }

        const QString workpieceId = m_workpieceIdEdit ? m_workpieceIdEdit->text().trimmed() : QString();
        static const QRegularExpression workpieceIdPattern(QStringLiteral("^\\d{6}$"));
        if (!workpieceIdPattern.match(workpieceId).hasMatch()) {
            QMessageBox::warning(this, QStringLiteral("工件 ID 错误"), QStringLiteral("请选择有效的六位数工件 ID。"));
            appendLog(QStringLiteral("<span style='color:#f38ba8;'>汇川启动失败：工件 ID \"%1\" 不是六位数字。</span>").arg(workpieceId));
            return;
        }

        QString variableName;
        if (!huichuanConfiguredWriteVariable(variableName)) {
            QMessageBox::warning(this, QStringLiteral("配置错误"),
                                 QStringLiteral("请在 %1 中配置 mode=write 的 global_variable 项。")
                                     .arg(connectionConfigPathForBrand(QStringLiteral("huichuan"))));
            appendLog(QStringLiteral("<span style='color:#f38ba8;'>汇川启动失败：未找到 mode=write 的全局变量配置。</span>"));
            return;
        }

        const Result writeResult = robot_set_global_variant(m_socketFd, variableName.toStdString(), workpieceId.toDouble());
        appendLog(QStringLiteral("汇川开始作业: 写入工件 ID 到全局变量 %1 = %2 -> %3")
                      .arg(variableName, workpieceId, resultToString(writeResult)));
        if (writeResult != SUCCESS) {
            const QString detail = QString::fromStdString(robot_last_global_variant_error()).trimmed();
            if (!detail.isEmpty()) {
                appendLog(QStringLiteral("<span style='color:#f9e2af;'>写入诊断: %1</span>").arg(detail));
            }
            QMessageBox::warning(this, QStringLiteral("写入失败"),
                                 QStringLiteral("工件 ID 写入全局变量 %1 失败：%2")
                                     .arg(variableName, resultToString(writeResult)));
            return;
        }
    } else if (name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("参数错误"), QStringLiteral("请先选择或输入作业文件名。"));
        return;
    } else {
        appendLog(QStringLiteral("<span style='color:#f9e2af;'>盈联开始作业: 使用作业名称 %1</span>").arg(name));
    }

    appendLog(QStringLiteral("<span style='color:#f9e2af;'>开始下发启动命令: %1</span>").arg(name));
    const Result result = robot_arm_actions::start_job(selectedRobotBrand(), m_socketFd, name.toStdString());
    appendLog(QStringLiteral("开始作业 %1 -> %2").arg(name, resultToString(result)));
    if (result == SUCCESS) {
        setTopJobStatus(QStringLiteral("运行中"), false);
    } else {
        std::vector<JobStartCheckItem> diagnostics;
        const Result diagnoseResult = diagnoseJobStartConditions(m_socketFd, diagnostics);
        appendLog(QStringLiteral("<span style='color:#f9e2af;'>启动前置条件诊断 -> %1</span>")
                      .arg(resultToString(diagnoseResult)));
        for (const JobStartCheckItem& item : diagnostics) {
            appendLog(QStringLiteral("<span style='color:#f9e2af;'>  %1: %2 (%3)</span>")
                          .arg(QString::fromStdString(item.name),
                               resultToString(item.result),
                               QString::fromStdString(item.detail)));
        }

        std::string currentPath;
        const Result pathResult = queryCurrentJobProgram(m_socketFd, currentPath);
        const QString currentProgram = pathResult == SUCCESS
            ? QString::fromLocal8Bit(currentPath.data(), static_cast<int>(currentPath.size()))
            : QStringLiteral("--");
        appendLog(QStringLiteral("<span style='color:#f38ba8;'>启动作业受限: 请求=\"%1\", 当前程序=\"%2\"。"
                                 "可能原因：工程名不匹配、未获取远程控制权、急停/报警、伺服未使能或控制器当前状态不允许启动。</span>")
                      .arg(name, currentProgram));
    }
}

void MainWindow::onTopJobPauseContinue()
{
    if (!ensureJobControllerReady(m_topJobPaused ? QStringLiteral("继续作业") : QStringLiteral("暂停作业"))) {
        return;
    }
    const bool wasPaused = m_topJobPaused;
    const Result result = wasPaused
        ? robot_arm_actions::continue_job(selectedRobotBrand(), m_socketFd)
        : robot_arm_actions::pause_job(selectedRobotBrand(), m_socketFd);
    appendLog(QStringLiteral("%1 -> %2").arg(wasPaused ? QStringLiteral("继续作业") : QStringLiteral("暂停作业"),
                                           resultToString(result)));
    if (result == SUCCESS) {
        setTopJobStatus(wasPaused ? QStringLiteral("运行中") : QStringLiteral("暂停"), !wasPaused);
    }
}

void MainWindow::onTopJobStop()
{
    if (!ensureJobControllerReady(QStringLiteral("停止作业"))) return;
    const Result result = robot_arm_actions::stop_job(selectedRobotBrand(), m_socketFd);
    appendLog(QStringLiteral("<span style='color:#f38ba8;'>停止作业 -> %1</span>").arg(resultToString(result)));
    if (result == SUCCESS) {
        setTopJobStatus(QStringLiteral("停止"), false);
    }
}

void MainWindow::onTopJobReset()
{
    if (!ensureJobControllerReady(QStringLiteral("复位作业"))) return;

    const QString name = normalizedJobName();
    if (name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("参数错误"), QStringLiteral("请先选择或输入作业文件名。"));
        return;
    }

    const Result result = robot_arm_actions::reset_job(selectedRobotBrand(), m_socketFd, name.toStdString());
    appendLog(QStringLiteral("复位作业: %1 -> %2").arg(name, resultToString(result)));
    if (result == SUCCESS) {
        setTopJobStatus(QStringLiteral("停止"), false);
    }
}

void MainWindow::onSetDigitalOutput()
{
    if (!m_connected) {
        return;
    }
    const Result result = robot_set_digital_output(m_socketFd, m_doPortSpin->value(), m_doValueCombo->currentIndex());
    appendLog(QStringLiteral("设置数字输出: DO%1 = %2 -> %3")
                  .arg(m_doPortSpin->value())
                  .arg(m_doValueCombo->currentIndex())
                  .arg(resultToString(result)));
}

void MainWindow::onRefreshIO()
{
    if (!m_connected) {
        return;
    }

    int failures = 0;
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 2; ++c) {
            const int port = r + c * 8 + 1;
            int diValue = 0;
            int doValue = 0;
            const Result diResult = robot_get_digital_input(m_socketFd, port, diValue);
            const Result doResult = robot_get_digital_output(m_socketFd, port, doValue);
            if (diResult != SUCCESS) {
                ++failures;
            }
            if (doResult != SUCCESS) {
                ++failures;
            }
            if (m_diTable && m_diTable->item(r, c * 2 + 1)) {
                m_diTable->item(r, c * 2 + 1)->setText(diResult == SUCCESS ? QString::number(diValue) : QStringLiteral("--"));
            }
            if (m_doTable && m_doTable->item(r, c * 2 + 1)) {
                m_doTable->item(r, c * 2 + 1)->setText(doResult == SUCCESS ? QString::number(doValue) : QStringLiteral("--"));
            }
        }
    }
    appendLog(QStringLiteral("刷新 IO 状态 -> %1").arg(failures == 0 ? QStringLiteral("SUCCESS") : QStringLiteral("部分失败")));
}

void MainWindow::onSetGlobalVariant()
{
    if (!m_connected) {
        appendLog(QStringLiteral("<span style='color:#f9e2af;'>写入全局变量失败：请先连接控制器。</span>"));
        return;
    }
    QString name;
    if (!currentGlobalVariantName(name)) {
        QMessageBox::warning(this, QStringLiteral("参数错误"), QStringLiteral("请输入合法的全局变量名，例如 GB009、GI000、GD000。"));
        appendLog(QStringLiteral("<span style='color:#f38ba8;'>写入全局变量失败：变量名不合法。</span>"));
        return;
    }

    const double value = m_globalVarValueSpin->value();
    appendLog(QStringLiteral("<span style='color:#f9e2af;'>准备写入全局变量: %1 = %2</span>").arg(name).arg(value));
    const Result result = robot_set_global_variant(m_socketFd, name.toStdString(), value);
    m_globalVarResultLabel->setText(QStringLiteral("结果: %1").arg(resultToString(result)));
    appendLog(QStringLiteral("写入全局变量 %1 = %2 -> %3")
                  .arg(name)
                  .arg(value)
                  .arg(resultToString(result)));
    if (result != SUCCESS) {
        const QString detail = QString::fromStdString(robot_last_global_variant_error()).trimmed();
        if (!detail.isEmpty()) {
            appendLog(QStringLiteral("<span style='color:#f9e2af;'>写入诊断: %1</span>").arg(detail));
        }
    }
}

void MainWindow::onGetGlobalVariant()
{
    if (!m_connected) {
        appendLog(QStringLiteral("<span style='color:#f9e2af;'>读取全局变量失败：请先连接控制器。</span>"));
        return;
    }
    QString name;
    if (!currentGlobalVariantName(name)) {
        QMessageBox::warning(this, QStringLiteral("参数错误"), QStringLiteral("请输入合法的全局变量名，例如 GB009、GI000、GD000。"));
        appendLog(QStringLiteral("<span style='color:#f38ba8;'>读取全局变量失败：变量名不合法。</span>"));
        return;
    }

    appendLog(QStringLiteral("<span style='color:#f9e2af;'>准备读取全局变量: %1</span>").arg(name));
    double value = 0.0;
    const Result result = robot_get_global_variant(m_socketFd, name.toStdString(), value);
    if (result == SUCCESS) {
        m_globalVarValueSpin->setValue(value);
    }
    m_globalVarResultLabel->setText(QStringLiteral("结果: %1").arg(resultToString(result)));
    appendLog(QStringLiteral("读取全局变量 %1 -> %2 (%3)").arg(name).arg(value).arg(resultToString(result)));
}

void MainWindow::onToggleWindGrinder()
{
    if (!m_connected) {
        return;
    }
    const bool turnOn = !m_windGrinderOn;
    const int value = turnOn ? 1 : 0;
    const Result result = robot_set_digital_output(m_socketFd, kWindGrinderPort, value);
    if (result == SUCCESS) {
        m_windGrinderOn = turnOn;
        m_windGrinderBtn->setText(turnOn ? QStringLiteral("关闭风磨") : QStringLiteral("启动风磨"));
    }
    appendLog(QStringLiteral("%1风磨 -> DO%2=%3 -> %4")
                  .arg(turnOn ? QStringLiteral("启动") : QStringLiteral("关闭"))
                  .arg(kWindGrinderPort)
                  .arg(value)
                  .arg(resultToString(result)));
}

void MainWindow::onTimerUpdate()
{
    if (!m_connected) {
        return;
    }
    if (!pollRobotState()) {
        // 连续多次轮询全部失败，判定连接已断开
        appendLog(QStringLiteral("<span style='color:#f38ba8;'>自动断开本地连接状态: socket=%1。</span>").arg(m_socketFd));
        appendLog(QStringLiteral("<span style='color:#f38ba8;'>与控制器的连接已丢失，自动断开。</span>"));
        stopTimers();
        m_connected = false;
        m_socketFd = -1;
        m_pollFailCount = 0;
        setConnectionUi(false);
        updateConnectionStatusLabel(QStringLiteral("● 连接已断开"), false);
        refreshStatusSummary();
        refreshGlobalVariantModeLabel();
        return;
    }
    refreshStatusSummary();
}

QWidget* MainWindow::createWorkpieceSelectionPanel()
{
    m_workpieceSelection = new WorkpieceSelectionWidget();
    m_workpieceSelection->addPage(QStringLiteral("全局变量"), createGlobalVariantPanel());
    connect(m_workpieceSelection, &WorkpieceSelectionWidget::workpieceProgramChosen,
            this, [this](const QString &workpieceId, const QString &programName) {
        if (m_workpieceIdEdit) {
            m_workpieceIdEdit->setText(workpieceId);
        }
        if (m_jobNameEdit) {
            m_jobNameEdit->setText(programName);
        }
        m_workpieceSelected = true;
        updateTopJobButtons();
        appendLog(QStringLiteral("工件 %1 已选择，作业名称设置为 %2").arg(workpieceId, programName));
    });
    return m_workpieceSelection;
}


