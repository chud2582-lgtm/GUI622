#include "autocalibrationwidget.h"

#include "cpp/interface/nrc_interface.h"

#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

constexpr int kMotionPollIntervalMs = 100;
constexpr int kVisiblePoseAxisCount = 6;
constexpr auto kSettingsOrg = "RobotControlUI";
constexpr auto kSettingsApp = "AutoCalibration";

QString colorTag(const QString& color, const QString& text)
{
    return QStringLiteral("<span style='color:%1;'>%2</span>").arg(color, text);
}

QString readLastJsonLine(const QString& text)
{
    QString normalized = text;
    normalized.replace(QChar('\r'), QChar('\n'));
    const QStringList lines = normalized.split(QChar('\n'), Qt::SkipEmptyParts);
    for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
        const QString trimmed = it->trimmed();
        if (trimmed.startsWith('{') && trimmed.endsWith('}')) {
            return trimmed;
        }
    }
    return QString();
}

} // namespace

AutoCalibrationWidget::AutoCalibrationWidget(QWidget *parent)
    : QWidget(parent)
{
    buildUi();

    m_motionPollTimer = new QTimer(this);
    m_motionPollTimer->setInterval(kMotionPollIntervalMs);
    connect(m_motionPollTimer, &QTimer::timeout, this, &AutoCalibrationWidget::onMotionPollTick);

    m_perPoseTimeoutTimer = new QTimer(this);
    m_perPoseTimeoutTimer->setSingleShot(true);
    connect(m_perPoseTimeoutTimer, &QTimer::timeout, this, &AutoCalibrationWidget::onPerPoseTimeout);

    m_captureProcess = new QProcess(this);
    m_captureProcess->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_captureProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &AutoCalibrationWidget::onCaptureProcessFinished);
    connect(m_captureProcess, &QProcess::readyReadStandardOutput,
            this, &AutoCalibrationWidget::onCaptureProcessReadStdout);
    connect(m_captureProcess, &QProcess::readyReadStandardError,
            this, &AutoCalibrationWidget::onCaptureProcessReadStderr);

    m_calibProcess = new QProcess(this);
    m_calibProcess->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_calibProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &AutoCalibrationWidget::onCalibProcessFinished);
    connect(m_calibProcess, &QProcess::readyReadStandardOutput,
            this, &AutoCalibrationWidget::onCalibProcessReadStdout);
    connect(m_calibProcess, &QProcess::readyReadStandardError,
            this, &AutoCalibrationWidget::onCalibProcessReadStderr);

    m_configPath = defaultWaypointsPath();
    m_captureScriptPath = defaultCaptureScriptPath();
    m_calibScriptPath = defaultCalibScriptPath();
    m_outputRoot = defaultOutputRoot();
    loadPathSelections();

    m_configPathEdit->setText(m_configPath);
    m_captureScriptEdit->setText(m_captureScriptPath);
    m_calibScriptEdit->setText(m_calibScriptPath);
    m_outputRootEdit->setText(m_outputRoot);

    QString err;
    if (!loadWaypoints(m_configPath, &err)) {
        appendSessionLog(colorTag(QStringLiteral("#f9e2af"),
                                  QStringLiteral("默认路径点未加载：%1").arg(err)));
    } else {
        rebuildTableFromWaypoints();
        appendSessionLog(QStringLiteral("已加载默认路径点 %1 个：%2")
                             .arg(m_waypoints.size())
                             .arg(QDir::toNativeSeparators(m_configPath)));
    }
}

// =============================================================================
// UI 鏋勫缓
// =============================================================================

void AutoCalibrationWidget::buildUi()
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setSpacing(8);

    root->addWidget(buildConfigGroup());
    root->addWidget(buildScriptGroup());
    // Keep motion parameter widgets alive for the existing execution logic,
    // but remove this corrupted section from the visible layout.
    QGroupBox *hiddenMotionGroup = buildMotionGroup();
    hiddenMotionGroup->hide();
    root->addWidget(buildControlGroup());
    root->addWidget(buildTableGroup(), 1);
    root->addWidget(buildLogGroup(), 1);
}

QGroupBox* AutoCalibrationWidget::buildConfigGroup()
{
    QGroupBox *box = new QGroupBox(QStringLiteral("路径配置"), this);
    QGridLayout *grid = new QGridLayout(box);

    grid->addWidget(new QLabel(QStringLiteral("配置文件:")), 0, 0);
    m_configPathEdit = new QLineEdit();
    grid->addWidget(m_configPathEdit, 0, 1);
    m_browseConfigBtn = new QPushButton(QStringLiteral("浏览"));
    grid->addWidget(m_browseConfigBtn, 0, 2);
    m_reloadConfigBtn = new QPushButton(QStringLiteral("重新加载"));
    grid->addWidget(m_reloadConfigBtn, 0, 3);

    connect(m_browseConfigBtn, &QPushButton::clicked, this, &AutoCalibrationWidget::onBrowseWaypoints);
    connect(m_reloadConfigBtn, &QPushButton::clicked, this, &AutoCalibrationWidget::onReloadWaypoints);
    connect(m_configPathEdit, &QLineEdit::textChanged, this,
            [this](const QString&) { savePathSelections(); });
    return box;
}

QGroupBox* AutoCalibrationWidget::buildScriptGroup()
{
    QGroupBox *box = new QGroupBox(QStringLiteral("脚本与输出目录"), this);
    QGridLayout *grid = new QGridLayout(box);

    grid->addWidget(new QLabel(QStringLiteral("Python 解释器:")), 0, 0);
    m_pythonExeEdit = new QLineEdit(m_pythonExe);
    m_pythonExeEdit->setPlaceholderText(QStringLiteral("python  /  D:/...env/Scripts/python.exe"));
    grid->addWidget(m_pythonExeEdit, 0, 1);
    m_browsePythonBtn = new QPushButton(QStringLiteral("浏览"));
    grid->addWidget(m_browsePythonBtn, 0, 2);
    m_testPythonBtn = new QPushButton(QStringLiteral("测试"));
    m_testPythonBtn->setProperty("styleClass", "primary");
    grid->addWidget(m_testPythonBtn, 0, 3);

    grid->addWidget(new QLabel(QStringLiteral("采图脚本:")), 1, 0);
    m_captureScriptEdit = new QLineEdit();
    grid->addWidget(m_captureScriptEdit, 1, 1);
    m_browseCaptureBtn = new QPushButton(QStringLiteral("浏览"));
    grid->addWidget(m_browseCaptureBtn, 1, 2);

    grid->addWidget(new QLabel(QStringLiteral("标定脚本:")), 2, 0);
    m_calibScriptEdit = new QLineEdit();
    grid->addWidget(m_calibScriptEdit, 2, 1);
    m_browseCalibBtn = new QPushButton(QStringLiteral("浏览"));
    grid->addWidget(m_browseCalibBtn, 2, 2);

    grid->addWidget(new QLabel(QStringLiteral("输出根目录:")), 3, 0);
    m_outputRootEdit = new QLineEdit();
    grid->addWidget(m_outputRootEdit, 3, 1);
    m_browseOutputBtn = new QPushButton(QStringLiteral("浏览"));
    grid->addWidget(m_browseOutputBtn, 3, 2);

    connect(m_browsePythonBtn, &QPushButton::clicked, this, &AutoCalibrationWidget::onBrowsePython);
    connect(m_testPythonBtn, &QPushButton::clicked, this, &AutoCalibrationWidget::onTestPython);
    connect(m_browseCaptureBtn, &QPushButton::clicked, this, &AutoCalibrationWidget::onBrowseCaptureScript);
    connect(m_browseCalibBtn, &QPushButton::clicked, this, &AutoCalibrationWidget::onBrowseCalibScript);
    connect(m_browseOutputBtn, &QPushButton::clicked, this, &AutoCalibrationWidget::onBrowseOutputDir);
    connect(m_pythonExeEdit, &QLineEdit::textChanged, this,
            [this](const QString&) { savePathSelections(); });
    connect(m_captureScriptEdit, &QLineEdit::textChanged, this,
            [this](const QString&) { savePathSelections(); });
    connect(m_calibScriptEdit, &QLineEdit::textChanged, this,
            [this](const QString&) { savePathSelections(); });
    connect(m_outputRootEdit, &QLineEdit::textChanged, this,
            [this](const QString&) { savePathSelections(); });
    return box;
}

QGroupBox* AutoCalibrationWidget::buildMotionGroup()
{
    QGroupBox *box = new QGroupBox(QStringLiteral("运动参数"), this);
    QGridLayout *grid = new QGridLayout(box);

    grid->addWidget(new QLabel(QStringLiteral("坐标系:")), 0, 0);
    m_coordCombo = new QComboBox();
    m_coordCombo->addItem(QStringLiteral("关节 (joint)"), static_cast<int>(Coord::Joint));
    m_coordCombo->addItem(QStringLiteral("笛卡尔 (cartesian)"), static_cast<int>(Coord::Cartesian));
    grid->addWidget(m_coordCombo, 0, 1);

    grid->addWidget(new QLabel(QStringLiteral("运动方式:")), 0, 2);
    m_moveTypeCombo = new QComboBox();
    m_moveTypeCombo->addItem(QStringLiteral("MoveJ"), static_cast<int>(MoveType::MoveJ));
    m_moveTypeCombo->addItem(QStringLiteral("MoveL"), static_cast<int>(MoveType::MoveL));
    grid->addWidget(m_moveTypeCombo, 0, 3);

    grid->addWidget(new QLabel(QStringLiteral("速度:")), 1, 0);
    m_velocitySpin = new QSpinBox();
    m_velocitySpin->setRange(1, 1000);
    m_velocitySpin->setValue(m_velocity);
    grid->addWidget(m_velocitySpin, 1, 1);

    grid->addWidget(new QLabel(QStringLiteral("加速度:")), 1, 2);
    m_accSpin = new QSpinBox();
    m_accSpin->setRange(1, 100);
    m_accSpin->setValue(m_acceleration);
    grid->addWidget(m_accSpin, 1, 3);

    grid->addWidget(new QLabel(QStringLiteral("减速度:")), 1, 4);
    m_decSpin = new QSpinBox();
    m_decSpin->setRange(1, 100);
    m_decSpin->setValue(m_deceleration);
    grid->addWidget(m_decSpin, 1, 5);

    grid->addWidget(new QLabel(QStringLiteral("稳定等待 (ms):")), 2, 0);
    m_settlingSpin = new QSpinBox();
    m_settlingSpin->setRange(0, 10000);
    m_settlingSpin->setValue(m_settlingMs);
    grid->addWidget(m_settlingSpin, 2, 1);

    grid->addWidget(new QLabel(QStringLiteral("鍗曠偣瓒呮椂 (ms):")), 2, 2);
    m_perPoseTimeoutSpin = new QSpinBox();
    m_perPoseTimeoutSpin->setRange(1000, 600000);
    m_perPoseTimeoutSpin->setValue(m_perPoseTimeoutMs);
    grid->addWidget(m_perPoseTimeoutSpin, 2, 3);

    return box;
}

QGroupBox* AutoCalibrationWidget::buildControlGroup()
{
    QGroupBox *box = new QGroupBox(QStringLiteral("自动标定执行"), this);
    QHBoxLayout *row = new QHBoxLayout(box);

    m_skipBtn = new QPushButton(QStringLiteral("跳过当前点"));
    m_skipBtn->setEnabled(false);
    row->addWidget(m_skipBtn);

    m_manualCaptureBtn = new QPushButton(QStringLiteral("采图"));
    m_manualCaptureBtn->setProperty("styleClass", "primary");
    row->addWidget(m_manualCaptureBtn);

    m_manualCalibBtn = new QPushButton(QStringLiteral("标定"));
    m_manualCalibBtn->setProperty("styleClass", "primary");
    row->addWidget(m_manualCalibBtn);

    row->addStretch();

    m_stateLabel = new QLabel(QStringLiteral("状态: 空闲"));
    row->addWidget(m_stateLabel);

    connect(m_skipBtn, &QPushButton::clicked, this, &AutoCalibrationWidget::onSkipClicked);
    connect(m_manualCaptureBtn, &QPushButton::clicked, this, &AutoCalibrationWidget::onManualCaptureClicked);
    connect(m_manualCalibBtn, &QPushButton::clicked, this, &AutoCalibrationWidget::onManualCalibClicked);

    return box;
}

QGroupBox* AutoCalibrationWidget::buildTableGroup()
{
    QGroupBox *box = new QGroupBox(QStringLiteral("路径点状态"), this);
    QVBoxLayout *layout = new QVBoxLayout(box);

    m_table = new QTableWidget();
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({QStringLiteral("#"),
                                        QStringLiteral("名称"),
                                        QStringLiteral("目标位姿"),
                                        QStringLiteral("状态"),
                                        QStringLiteral("反馈")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table);
    return box;
}

QGroupBox* AutoCalibrationWidget::buildLogGroup()
{
    QGroupBox *box = new QGroupBox(QStringLiteral("会话日志"), this);
    QVBoxLayout *layout = new QVBoxLayout(box);
    m_sessionLog = new QTextEdit();
    m_sessionLog->setReadOnly(true);
    m_sessionLog->setMaximumHeight(160);
    layout->addWidget(m_sessionLog);
    return box;
}

// =============================================================================
// 娴忚 / 閲嶆柊鍔犺浇
// =============================================================================

void AutoCalibrationWidget::onBrowseWaypoints()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择路径点 JSON 配置"),
        m_configPathEdit->text(), QStringLiteral("JSON (*.json)"));
    if (!path.isEmpty()) {
        m_configPathEdit->setText(path);
        onReloadWaypoints();
    }
}

void AutoCalibrationWidget::onReloadWaypoints()
{
    const QString path = m_configPathEdit->text().trimmed();
    QString err;
    if (!loadWaypoints(path, &err)) {
        appendSessionLog(colorTag(QStringLiteral("#f38ba8"),
                                  QStringLiteral("加载路径点失败：%1").arg(err)));
        return;
    }
    rebuildTableFromWaypoints();
    appendSessionLog(QStringLiteral("已加载路径点 %1 个：%2")
                         .arg(m_waypoints.size())
                         .arg(QDir::toNativeSeparators(path)));
}

void AutoCalibrationWidget::onBrowseCaptureScript()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择采图脚本"),
        m_captureScriptEdit->text(),
        QStringLiteral("Python (*.py);;All (*.*)"));
    if (!path.isEmpty()) {
        m_captureScriptEdit->setText(path);
    }
}

void AutoCalibrationWidget::onBrowseCalibScript()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择标定脚本"),
        m_calibScriptEdit->text(),
        QStringLiteral("Python (*.py);;All (*.*)"));
    if (!path.isEmpty()) {
        m_calibScriptEdit->setText(path);
    }
}

void AutoCalibrationWidget::onBrowseOutputDir()
{
    const QString path = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择输出根目录"), m_outputRootEdit->text());
    if (!path.isEmpty()) {
        m_outputRootEdit->setText(path);
    }
}

void AutoCalibrationWidget::onBrowsePython()
{
#ifdef Q_OS_WIN
    const QString filter = QStringLiteral("Python 解释器 (python.exe pythonw.exe);;所有文件 (*.*)");
#else
    const QString filter = QStringLiteral("可执行文件 (*);;所有文件 (*)");
#endif
    QString start = m_pythonExeEdit->text().trimmed();
    if (start.isEmpty() || start == QStringLiteral("python") || start == QStringLiteral("python3")) {
        start = QString();
    }
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择 Python 解释器 (python.exe)"),
        start, filter);
    if (!path.isEmpty()) {
        m_pythonExeEdit->setText(QDir::toNativeSeparators(path));
    }
}

void AutoCalibrationWidget::onTestPython()
{
    QString exe = m_pythonExeEdit->text().trimmed();
    if (exe.isEmpty()) {
        exe = QStringLiteral("python");
        m_pythonExeEdit->setText(exe);
    }
    appendSessionLog(QStringLiteral("[测试 Python] 解释器: %1").arg(exe));

    auto runOnce = [this, &exe](const QStringList& args, int timeoutMs) -> std::pair<int, QString> {
        QProcess proc;
        proc.setProcessChannelMode(QProcess::MergedChannels);
        proc.start(exe, args);
        if (!proc.waitForStarted(2000)) {
            return {-1, QStringLiteral("无法启动，请确认解释器路径可执行: %1").arg(exe)};
        }
        if (!proc.waitForFinished(timeoutMs)) {
            proc.kill();
            proc.waitForFinished(500);
            return {-2, QStringLiteral("超时: %1 ms").arg(timeoutMs)};
        }
        const QString output = QString::fromLocal8Bit(proc.readAll()).trimmed();
        return {proc.exitCode(), output};
    };

    // 1) python --version
    auto [verCode, verOut] = runOnce({QStringLiteral("--version")}, 5000);
    if (verCode < 0) {
        appendSessionLog(colorTag(QStringLiteral("#f38ba8"),
                                  QStringLiteral("解释器不可用: %1").arg(verOut)));
        return;
    }
    if (verCode != 0) {
        appendSessionLog(colorTag(QStringLiteral("#f38ba8"),
                                  QStringLiteral("python --version 退出码=%1，输出: %2")
                                      .arg(verCode).arg(verOut)));
        return;
    }
    appendSessionLog(colorTag(QStringLiteral("#a6e3a1"),
                              QStringLiteral("%1").arg(verOut.isEmpty()
                                  ? QStringLiteral("解释器可用") : verOut)));

    // 2) 检查 ChArUco 依赖：opencv-contrib-python（cv2.aruco）
    const QStringList probe = {
        QStringLiteral("-c"),
        QStringLiteral(
            "import sys\n"
            "try:\n"
            "    import cv2\n"
            "    has_aruco = hasattr(cv2, 'aruco')\n"
            "    print('cv2 ' + cv2.__version__ + ('  aruco=YES' if has_aruco else '  aruco=NO (need opencv-contrib-python)'))\n"
            "    sys.exit(0 if has_aruco else 3)\n"
            "except ImportError as e:\n"
            "    print('cv2 ImportError: ' + str(e))\n"
            "    sys.exit(2)\n")
    };
    auto [cvCode, cvOut] = runOnce(probe, 15000);
    if (cvCode == 0) {
        appendSessionLog(colorTag(QStringLiteral("#a6e3a1"),
                                  QStringLiteral("%1").arg(cvOut)));
        appendSessionLog(QStringLiteral("Python 解释器测试通过，可用于自动标定。"));
    } else if (cvCode == 2) {
        appendSessionLog(colorTag(QStringLiteral("#f9e2af"),
                                  QStringLiteral("当前解释器未安装 cv2: %1").arg(cvOut)));
        appendSessionLog(QStringLiteral("请在该环境执行: pip install opencv-contrib-python"));
    } else if (cvCode == 3) {
        appendSessionLog(colorTag(QStringLiteral("#f9e2af"),
                                  QStringLiteral("%1").arg(cvOut)));
        appendSessionLog(QStringLiteral("当前安装的是 opencv-python，缺少 aruco 模块。请改装: pip install opencv-contrib-python"));
    } else {
        appendSessionLog(colorTag(QStringLiteral("#f38ba8"),
                                  QStringLiteral("cv2 检测失败 (exit=%1): %2").arg(cvCode).arg(cvOut)));
    }
}

// =============================================================================
// 榛樿璺緞
// =============================================================================

void AutoCalibrationWidget::onManualCaptureClicked()
{
    std::array<double, 7> jointPose = {};
    std::array<double, 7> cartPose = {};
    QString poseError;
    if (!queryCurrentPoseForRecord(&jointPose, &cartPose, &poseError)) {
        QMessageBox::warning(this, QStringLiteral("自动标定"),
                             poseError.isEmpty()
                                 ? QStringLiteral("无法读取当前位姿，请确认控制器已连接。")
                                 : poseError);
        return;
    }
    m_curJoint = jointPose;
    m_curCart = cartPose;

    if (!startVerificationProcess(m_captureProcess,
                                  verificationCaptureScriptPath(),
                                  QStringLiteral("采图"),
                                  QStringLiteral("状态: 采图验证中"),
                                  true)) {
        return;
    }

    QString savedPath;
    if (appendManualCaptureSnapshot(jointPose, cartPose, &savedPath)) {
        appendSessionLog(QStringLiteral("手动采图记录已写入: %1")
                             .arg(QDir::toNativeSeparators(savedPath)));
    } else {
        appendSessionLog(colorTag(QStringLiteral("#f38ba8"),
                                  QStringLiteral("手动采图记录写入失败。")));
    }
}

void AutoCalibrationWidget::onManualCalibClicked()
{
    startVerificationProcess(m_calibProcess,
                             verificationCalibScriptPath(),
                             QStringLiteral("标定"),
                             QStringLiteral("状态: 标定验证中"),
                             false);
}

QString AutoCalibrationWidget::defaultWaypointsPath() const
{
    const QString here = QApplication::applicationDirPath();
    QStringList candidates = {
        QDir(here).absoluteFilePath(QStringLiteral("calib_scripts/calib_waypoints.json")),
        QDir(here).absoluteFilePath(QStringLiteral("../calib_scripts/calib_waypoints.json")),
        QDir(here).absoluteFilePath(QStringLiteral("../../calib_scripts/calib_waypoints.json"))
    };
    for (const QString& c : candidates) {
        if (QFileInfo::exists(c)) {
            return QDir::cleanPath(c);
        }
    }
    return candidates.first();
}

QString AutoCalibrationWidget::defaultCaptureScriptPath() const
{
    const QString here = QApplication::applicationDirPath();
    QStringList candidates = {
        QDir(here).absoluteFilePath(QStringLiteral("calib_scripts/capture_charuco.py")),
        QDir(here).absoluteFilePath(QStringLiteral("../calib_scripts/capture_charuco.py")),
        QDir(here).absoluteFilePath(QStringLiteral("../../calib_scripts/capture_charuco.py"))
    };
    for (const QString& c : candidates) {
        if (QFileInfo::exists(c)) {
            return QDir::cleanPath(c);
        }
    }
    return candidates.first();
}

QString AutoCalibrationWidget::defaultCalibScriptPath() const
{
    const QString here = QApplication::applicationDirPath();
    QStringList candidates = {
        QDir(here).absoluteFilePath(QStringLiteral("calib_scripts/run_calibration.py")),
        QDir(here).absoluteFilePath(QStringLiteral("../calib_scripts/run_calibration.py")),
        QDir(here).absoluteFilePath(QStringLiteral("../../calib_scripts/run_calibration.py"))
    };
    for (const QString& c : candidates) {
        if (QFileInfo::exists(c)) {
            return QDir::cleanPath(c);
        }
    }
    return candidates.first();
}

QString AutoCalibrationWidget::defaultOutputRoot() const
{
    return QDir(QApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("calib_sessions"));
}

QString AutoCalibrationWidget::verificationCaptureScriptPath() const
{
    const QString here = QApplication::applicationDirPath();
    QStringList candidates = {
        QDir(here).absoluteFilePath(QStringLiteral("calib_scripts/verify_capture.py")),
        QDir(here).absoluteFilePath(QStringLiteral("../calib_scripts/verify_capture.py")),
        QDir(here).absoluteFilePath(QStringLiteral("../../calib_scripts/verify_capture.py"))
    };
    for (const QString& c : candidates) {
        if (QFileInfo::exists(c)) {
            return QDir::cleanPath(c);
        }
    }
    return candidates.first();
}

QString AutoCalibrationWidget::verificationCalibScriptPath() const
{
    const QString here = QApplication::applicationDirPath();
    QStringList candidates = {
        QDir(here).absoluteFilePath(QStringLiteral("calib_scripts/verify_calibration.py")),
        QDir(here).absoluteFilePath(QStringLiteral("../calib_scripts/verify_calibration.py")),
        QDir(here).absoluteFilePath(QStringLiteral("../../calib_scripts/verify_calibration.py"))
    };
    for (const QString& c : candidates) {
        if (QFileInfo::exists(c)) {
            return QDir::cleanPath(c);
        }
    }
    return candidates.first();
}

bool AutoCalibrationWidget::startVerificationProcess(QProcess* process,
                                                     const QString& scriptPath,
                                                     const QString& actionName,
                                                     const QString& stateText,
                                                     bool isCapture)
{
    if (m_state != State::Idle && m_state != State::FinishedAll) {
        QMessageBox::information(this, QStringLiteral("自动标定"),
                                 QStringLiteral("当前有任务在运行，请等待完成后再执行%1。").arg(actionName));
        return false;
    }
    if (process->state() != QProcess::NotRunning) {
        QMessageBox::information(this, QStringLiteral("自动标定"),
                                 QStringLiteral("%1脚本仍在运行中。").arg(actionName));
        return false;
    }
    if (!QFileInfo::exists(scriptPath)) {
        QMessageBox::warning(this, QStringLiteral("自动标定"),
                             QStringLiteral("%1验证脚本不存在: %2").arg(actionName, scriptPath));
        return false;
    }

    m_pythonExe = m_pythonExeEdit->text().trimmed();
    if (m_pythonExe.isEmpty()) {
        m_pythonExe = QStringLiteral("python");
        m_pythonExeEdit->setText(m_pythonExe);
    }

    const QStringList args = {scriptPath};
    appendSessionLog(QStringLiteral("启动%1验证: %2 %3")
                         .arg(actionName, m_pythonExe, args.join(' ')));

    process->setWorkingDirectory(QFileInfo(scriptPath).absolutePath());
    process->start(m_pythonExe, args);
    if (!process->waitForStarted(3000)) {
        QMessageBox::warning(this, QStringLiteral("自动标定"),
                             QStringLiteral("%1验证脚本启动失败，请检查 Python 和脚本路径。").arg(actionName));
        return false;
    }

    m_state = isCapture ? State::Capturing : State::Calibrating;
    m_stateLabel->setText(stateText);
    m_manualCaptureActive = isCapture;
    m_manualCalibActive = !isCapture;
    m_manualCaptureBtn->setEnabled(false);
    m_manualCalibBtn->setEnabled(false);
    return true;
}

void AutoCalibrationWidget::loadPathSelections()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QString::fromLatin1(kSettingsOrg),
                       QString::fromLatin1(kSettingsApp));

    const QString configPath = settings.value(QStringLiteral("paths/config"), m_configPath).toString().trimmed();
    const QString pythonPath = settings.value(QStringLiteral("paths/python"), m_pythonExe).toString().trimmed();
    const QString captureScript = settings.value(QStringLiteral("paths/capture_script"), m_captureScriptPath).toString().trimmed();
    const QString calibScript = settings.value(QStringLiteral("paths/calib_script"), m_calibScriptPath).toString().trimmed();
    const QString outputRoot = settings.value(QStringLiteral("paths/output_root"), m_outputRoot).toString().trimmed();

    if (!configPath.isEmpty()) {
        m_configPath = configPath;
    }
    if (!pythonPath.isEmpty()) {
        m_pythonExe = pythonPath;
    }
    if (!captureScript.isEmpty()) {
        m_captureScriptPath = captureScript;
    }
    if (!calibScript.isEmpty()) {
        m_calibScriptPath = calibScript;
    }
    if (!outputRoot.isEmpty()) {
        m_outputRoot = outputRoot;
    }
}

void AutoCalibrationWidget::savePathSelections() const
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QString::fromLatin1(kSettingsOrg),
                       QString::fromLatin1(kSettingsApp));

    settings.setValue(QStringLiteral("paths/config"),
                      m_configPathEdit ? m_configPathEdit->text().trimmed() : m_configPath);
    settings.setValue(QStringLiteral("paths/python"),
                      m_pythonExeEdit ? m_pythonExeEdit->text().trimmed() : m_pythonExe);
    settings.setValue(QStringLiteral("paths/capture_script"),
                      m_captureScriptEdit ? m_captureScriptEdit->text().trimmed() : m_captureScriptPath);
    settings.setValue(QStringLiteral("paths/calib_script"),
                      m_calibScriptEdit ? m_calibScriptEdit->text().trimmed() : m_calibScriptPath);
    settings.setValue(QStringLiteral("paths/output_root"),
                      m_outputRootEdit ? m_outputRootEdit->text().trimmed() : m_outputRoot);
    settings.sync();
}

// =============================================================================
// 鍔犺浇 waypoints.json
// =============================================================================

bool AutoCalibrationWidget::loadWaypoints(const QString& path, QString* errorOut)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (errorOut) *errorOut = QStringLiteral("无法打开文件：%1").arg(path);
        return false;
    }
    const QByteArray bytes = f.readAll();
    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseErr);
    if (doc.isNull() || !doc.isObject()) {
        if (errorOut) *errorOut = QStringLiteral("JSON 解析错误：%1").arg(parseErr.errorString());
        return false;
    }
    const QJsonObject root = doc.object();

    const QString coordStr = root.value(QStringLiteral("coord")).toString(QStringLiteral("joint"));
    m_coord = (coordStr.compare(QStringLiteral("cartesian"), Qt::CaseInsensitive) == 0)
                  ? Coord::Cartesian : Coord::Joint;
    const QString moveTypeStr = root.value(QStringLiteral("move_type")).toString(QStringLiteral("movej"));
    m_moveType = (moveTypeStr.compare(QStringLiteral("movel"), Qt::CaseInsensitive) == 0)
                     ? MoveType::MoveL : MoveType::MoveJ;

    m_velocity = root.value(QStringLiteral("default_velocity")).toInt(m_velocity);
    m_acceleration = root.value(QStringLiteral("default_acceleration")).toInt(m_acceleration);
    m_deceleration = root.value(QStringLiteral("default_deceleration")).toInt(m_deceleration);
    m_settlingMs = root.value(QStringLiteral("settling_ms")).toInt(m_settlingMs);
    m_perPoseTimeoutMs = root.value(QStringLiteral("per_pose_timeout_ms")).toInt(m_perPoseTimeoutMs);

    const QJsonObject tol = root.value(QStringLiteral("pose_tolerance")).toObject();
    m_jointTolDeg = tol.value(QStringLiteral("joint_deg")).toDouble(m_jointTolDeg);
    m_linearTolMm = tol.value(QStringLiteral("linear_mm")).toDouble(m_linearTolMm);
    m_angularTolDeg = tol.value(QStringLiteral("angular_deg")).toDouble(m_angularTolDeg);

    const QJsonArray arr = root.value(QStringLiteral("waypoints")).toArray();
    m_waypoints.clear();
    m_waypoints.reserve(arr.size());
    for (const QJsonValue& v : arr) {
        const QJsonObject wpObj = v.toObject();
        const QJsonArray poseArr = wpObj.value(QStringLiteral("pose")).toArray();
        if (poseArr.size() < 6) {
            continue;
        }
        Waypoint wp;
        wp.id = wpObj.value(QStringLiteral("id")).toInt(static_cast<int>(m_waypoints.size()) + 1);
        wp.name = wpObj.value(QStringLiteral("name")).toString(QStringLiteral("WP_%1").arg(wp.id, 2, 10, QChar('0')));
        wp.velocity = wpObj.value(QStringLiteral("velocity")).toDouble(0.0);
        for (int i = 0; i < 7; ++i) {
            wp.pose[i] = (i < poseArr.size()) ? poseArr.at(i).toDouble(0.0) : 0.0;
        }
        wp.pose[6] = 0.0;
        m_waypoints.push_back(wp);
    }

    // 鎶婂姞杞藉埌鐨勯粯璁ゅ€煎悓姝ュ埌 UI
    if (m_coordCombo) m_coordCombo->setCurrentIndex(static_cast<int>(m_coord));
    if (m_moveTypeCombo) m_moveTypeCombo->setCurrentIndex(static_cast<int>(m_moveType));
    if (m_velocitySpin) m_velocitySpin->setValue(m_velocity);
    if (m_accSpin) m_accSpin->setValue(m_acceleration);
    if (m_decSpin) m_decSpin->setValue(m_deceleration);
    if (m_settlingSpin) m_settlingSpin->setValue(m_settlingMs);
    if (m_perPoseTimeoutSpin) m_perPoseTimeoutSpin->setValue(m_perPoseTimeoutMs);

    m_configPath = path;
    if (errorOut) errorOut->clear();
    return true;
}

void AutoCalibrationWidget::rebuildTableFromWaypoints()
{
    m_table->setRowCount(static_cast<int>(m_waypoints.size()));
    for (int row = 0; row < static_cast<int>(m_waypoints.size()); ++row) {
        const Waypoint& wp = m_waypoints[row];
        m_table->setItem(row, 0, new QTableWidgetItem(QString::number(wp.id)));
        m_table->setItem(row, 1, new QTableWidgetItem(wp.name));
        m_table->setItem(row, 2, new QTableWidgetItem(poseToCsv(wp.pose)));
        m_table->setItem(row, 3, new QTableWidgetItem(QStringLiteral("待执行")));
        m_table->setItem(row, 4, new QTableWidgetItem(QString()));
    }
    if (m_progressBar) {
        m_progressBar->setRange(0, static_cast<int>(m_waypoints.size()));
        m_progressBar->setValue(0);
    }
    if (m_progressLabel) {
        m_progressLabel->setText(QStringLiteral("0 / %1").arg(m_waypoints.size()));
    }
}

// =============================================================================
// 杩炴帴 / 鐘舵€佸悓姝?// =============================================================================

void AutoCalibrationWidget::setConnectionState(bool connected, bool demoMode, SOCKETFD socketFd)
{
    m_connected = connected;
    m_demoMode = demoMode;
    m_socketFd = socketFd;
}

void AutoCalibrationWidget::updateRobotState(const std::array<double, 7>& joint,
                                             const std::array<double, 7>& cartesian,
                                             int runState)
{
    m_curJoint = joint;
    m_curCart = cartesian;
    m_curJoint[6] = 0.0;
    m_curCart[6] = 0.0;
    m_runState = runState;
}

// =============================================================================
// 鍚姩 / 鏆傚仠 / 璺宠繃 / 鍋滄
// =============================================================================

void AutoCalibrationWidget::onStartClicked()
{
    if (m_state != State::Idle && m_state != State::FinishedAll) {
        return;
    }
    if (m_waypoints.empty()) {
        QMessageBox::warning(this, QStringLiteral("自动标定"),
                             QStringLiteral("尚未加载路径点配置。"));
        return;
    }
    if (!m_connected) {
        QMessageBox::warning(this, QStringLiteral("自动标定"),
                             QStringLiteral("请先连接控制器（演示模式也可以）。"));
        return;
    }

    m_pythonExe = m_pythonExeEdit->text().trimmed();
    if (m_pythonExe.isEmpty()) m_pythonExe = QStringLiteral("python");
    m_captureScriptPath = m_captureScriptEdit->text().trimmed();
    m_calibScriptPath = m_calibScriptEdit->text().trimmed();
    m_outputRoot = m_outputRootEdit->text().trimmed();
    m_coord = static_cast<Coord>(m_coordCombo->currentData().toInt());
    m_moveType = static_cast<MoveType>(m_moveTypeCombo->currentData().toInt());
    m_velocity = m_velocitySpin->value();
    m_acceleration = m_accSpin->value();
    m_deceleration = m_decSpin->value();
    m_settlingMs = m_settlingSpin->value();
    m_perPoseTimeoutMs = m_perPoseTimeoutSpin->value();

    if (!QFileInfo::exists(m_captureScriptPath)) {
        QMessageBox::warning(this, QStringLiteral("自动标定"),
                             QStringLiteral("采图脚本不存在：%1").arg(m_captureScriptPath));
        return;
    }
    if (m_outputRoot.isEmpty()) {
        m_outputRoot = defaultOutputRoot();
        m_outputRootEdit->setText(m_outputRoot);
    }

    m_sessionId = sessionTimestamp();
    m_sessionDir = QDir(m_outputRoot).absoluteFilePath(m_sessionId);
    if (!QDir().mkpath(m_sessionDir)) {
        QMessageBox::warning(this, QStringLiteral("自动标定"),
                             QStringLiteral("无法创建会话目录：%1").arg(m_sessionDir));
        return;
    }

    appendSessionLog(colorTag(QStringLiteral("#a6e3a1"),
                              QStringLiteral("【会话开始】%1（共 %2 个路径点）会话目录：%3")
                                  .arg(m_sessionId)
                                  .arg(m_waypoints.size())
                                  .arg(QDir::toNativeSeparators(m_sessionDir))));

    m_captureRecords = QJsonArray();
    m_currentIndex = -1;
    m_pauseRequested = false;
    rebuildTableFromWaypoints();
    setControlsEnabled(true);

    startNextPose();
}

void AutoCalibrationWidget::onPauseClicked()
{
    if (m_state == State::Idle || m_state == State::FinishedAll) {
        return;
    }
    m_pauseRequested = !m_pauseRequested;
    if (m_pauseBtn) {
        m_pauseBtn->setText(m_pauseRequested ? QStringLiteral("继续") : QStringLiteral("暂停"));
    }
    appendSessionLog(m_pauseRequested
                         ? QStringLiteral("已请求暂停，将在当前路径点完成后停下。")
                         : QStringLiteral("已恢复执行。"));
    if (!m_pauseRequested && m_state == State::BetweenPoses) {
        startNextPose();
    }
}

void AutoCalibrationWidget::onSkipClicked()
{
    if (m_state == State::Idle || m_state == State::FinishedAll) {
        return;
    }
    appendSessionLog(QStringLiteral("用户请求跳过当前路径点。"));

    if (m_captureProcess->state() != QProcess::NotRunning) {
        m_captureProcess->kill();
    }
    m_motionPollTimer->stop();
    m_perPoseTimeoutTimer->stop();

    if (m_currentIndex >= 0 && m_currentIndex < static_cast<int>(m_waypoints.size())) {
        QJsonObject info;
        info["accepted"] = false;
        info["reason"] = QStringLiteral("user_skip");
        onPoseRejected(QStringLiteral("鐢ㄦ埛璺宠繃"), info);
    }
}

void AutoCalibrationWidget::onStopClicked()
{
    if (m_state == State::Idle) {
        return;
    }
    appendSessionLog(colorTag(QStringLiteral("#f38ba8"),
                              QStringLiteral("用户请求停止本次会话。")));
    m_state = State::Stopping;
    if (m_captureProcess->state() != QProcess::NotRunning) {
        m_captureProcess->kill();
    }
    m_motionPollTimer->stop();
    m_perPoseTimeoutTimer->stop();

    finalizeAndDumpCaptures();
    setControlsEnabled(false);
    m_state = State::Idle;
    m_stateLabel->setText(QStringLiteral("状态: 已停止"));
}

void AutoCalibrationWidget::onLaunchCalibrationClicked()
{
    if (m_state != State::Idle && m_state != State::FinishedAll) {
        QMessageBox::information(this, QStringLiteral("自动标定"),
                                 QStringLiteral("当前有任务在运行，请等待完成后再启动标定。"));
        return;
    }

    m_calibScriptPath = m_calibScriptEdit->text().trimmed();
    if (!QFileInfo::exists(m_calibScriptPath)) {
        QMessageBox::warning(this, QStringLiteral("自动标定"),
                             QStringLiteral("标定脚本不存在: %1").arg(m_calibScriptPath));
        return;
    }
    if (m_sessionDir.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("自动标定"),
                             QStringLiteral("尚未运行采集会话，无可用 captures.json。"));
        return;
    }

    const QString capturesPath = QDir(m_sessionDir).absoluteFilePath(QStringLiteral("captures.json"));
    if (!QFileInfo::exists(capturesPath)) {
        QMessageBox::warning(this, QStringLiteral("自动标定"),
                             QStringLiteral("captures.json 不存在: %1").arg(capturesPath));
        return;
    }

    const QString outPath = QDir(m_sessionDir).absoluteFilePath(QStringLiteral("calibration_result.json"));
    const QStringList args = {
        m_calibScriptPath,
        QStringLiteral("--captures"), capturesPath,
        QStringLiteral("--output"), outPath,
        QStringLiteral("--session-id"), m_sessionId
    };
    appendSessionLog(QStringLiteral("启动离线标定: %1 %2")
                         .arg(m_pythonExe, args.join(' ')));
    m_state = State::Calibrating;
    m_stateLabel->setText(QStringLiteral("状态: 离线标定中"));
    m_calibProcess->setWorkingDirectory(m_sessionDir);
    m_calibProcess->start(m_pythonExe, args);
}

// =============================================================================
// 涓荤姸鎬佹満
// =============================================================================

void AutoCalibrationWidget::startNextPose()
{
    if (m_pauseRequested) {
        m_state = State::BetweenPoses;
        m_stateLabel->setText(QStringLiteral("状态: 已暂停"));
        return;
    }

    m_currentIndex += 1;
    if (m_currentIndex >= static_cast<int>(m_waypoints.size())) {
        m_state = State::FinishedAll;
        m_stateLabel->setText(QStringLiteral("状态: 全部采集完成"));
        appendSessionLog(colorTag(QStringLiteral("#a6e3a1"),
                                  QStringLiteral("【会话完成】共采集 %1 个路径点。")
                                      .arg(m_waypoints.size())));
        finalizeAndDumpCaptures();
        if (m_launchCalibBtn) {
            m_launchCalibBtn->setEnabled(true);
        }
        if (m_pauseBtn) {
            m_pauseBtn->setEnabled(false);
        }
        m_skipBtn->setEnabled(false);
        if (m_stopBtn) {
            m_stopBtn->setEnabled(false);
        }
        if (m_startBtn) {
            m_startBtn->setEnabled(true);
        }
        return;
    }

    const Waypoint& wp = m_waypoints[m_currentIndex];
    setRowStatus(m_currentIndex, QStringLiteral("移动中"), QStringLiteral("#f9e2af"));
    setRowDetail(m_currentIndex, QString());
    if (m_progressLabel) {
        m_progressLabel->setText(QStringLiteral("%1 / %2")
                                     .arg(m_currentIndex + 1).arg(m_waypoints.size()));
    }
    if (m_progressBar) {
        m_progressBar->setValue(m_currentIndex);
    }
    m_table->scrollToItem(m_table->item(m_currentIndex, 0));
    appendSessionLog(QStringLiteral("[%1/%2] 移动到 %3 -> [%4]")
                         .arg(m_currentIndex + 1)
                         .arg(m_waypoints.size())
                         .arg(wp.name, poseToCsv(wp.pose)));

    issueMoveCommand(wp);
}

void AutoCalibrationWidget::issueMoveCommand(const Waypoint& wp)
{
    m_state = State::Moving;
    m_stateLabel->setText(QStringLiteral("状态: 移动中"));
    m_motionStartedAtMs = QDateTime::currentMSecsSinceEpoch();

    if (m_demoMode) {
        // 婕旂ず妯″紡锛氳烦杩囧疄闄呬笅鍙戯紝鍋囪"浼氱Щ鍔ㄥ埌鐩爣浣嶅Э"锛屼笅涓€甯х敱绋冲畾璁℃椂鐩存帴杩涘叆閲囬泦
        m_motionPollTimer->start();
        m_perPoseTimeoutTimer->start(m_perPoseTimeoutMs);
        // 婕旂ず妯″紡鐩存帴鎶婂綋鍓嶄綅濮胯鎴愮洰鏍囷紝鍔犲揩娴佺▼
        m_curJoint = wp.pose;
        m_curCart = wp.pose;
        m_runState = 0;
        return;
    }

    MoveCmd cmd{};
    cmd.targetPosType = PosType::data;
    cmd.coord = static_cast<int>(m_coord);
    cmd.velocity = wp.velocity > 0.0 ? wp.velocity : m_velocity;
    cmd.acc = m_acceleration;
    cmd.dec = m_deceleration;
    for (int i = 0; i < 7; ++i) {
        cmd.targetPosValue[static_cast<size_t>(i)] = wp.pose[i];
    }
    cmd.targetPosValue[6] = 0.0;

    Result r = (m_moveType == MoveType::MoveL)
                   ? robot_movel(m_socketFd, cmd)
                   : robot_movej(m_socketFd, cmd);
    if (r != SUCCESS) {
        const QString msg = QStringLiteral("下发运动指令失败: code=%1").arg(static_cast<int>(r));
        setRowStatus(m_currentIndex, QStringLiteral("失败"), QStringLiteral("#f38ba8"));
        setRowDetail(m_currentIndex, msg);
        appendSessionLog(colorTag(QStringLiteral("#f38ba8"), msg));
        QJsonObject info;
        info["accepted"] = false;
        info["reason"] = QStringLiteral("move_command_failed");
        onPoseRejected(msg, info);
        return;
    }

    m_motionPollTimer->start();
    m_perPoseTimeoutTimer->start(m_perPoseTimeoutMs);
}

void AutoCalibrationWidget::onMotionPollTick()
{
    if (m_state != State::Moving) {
        return;
    }
    if (m_currentIndex < 0 || m_currentIndex >= static_cast<int>(m_waypoints.size())) {
        return;
    }
    const Waypoint& wp = m_waypoints[m_currentIndex];

    bool reached = false;
    if (m_demoMode) {
        // 演示模式下，稳定一小段时间就视为到位。
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        reached = (now - m_motionStartedAtMs) >= 250;
    } else {
        const bool stopped = (m_runState == 0);
        const bool atTarget = poseReached(wp);
        reached = stopped && atTarget;
    }
    if (reached) {
        m_motionPollTimer->stop();
        enterSettling();
    }
}

bool AutoCalibrationWidget::poseReached(const Waypoint& wp) const
{
    const auto& cur = isJointCoord() ? m_curJoint : m_curCart;
    if (isJointCoord()) {
        for (int i = 0; i < kVisiblePoseAxisCount; ++i) {
            if (std::abs(cur[i] - wp.pose[i]) > m_jointTolDeg) {
                return false;
            }
        }
        return true;
    }
    // 直角：前 3 位 mm，后 3 位 deg
    for (int i = 0; i < 3; ++i) {
        if (std::abs(cur[i] - wp.pose[i]) > m_linearTolMm) {
            return false;
        }
    }
    for (int i = 3; i < kVisiblePoseAxisCount; ++i) {
        if (std::abs(cur[i] - wp.pose[i]) > m_angularTolDeg) {
            return false;
        }
    }
    return true;
}

void AutoCalibrationWidget::enterSettling()
{
    m_state = State::Settling;
    m_stateLabel->setText(QStringLiteral("状态: 稳定中"));
    setRowStatus(m_currentIndex, QStringLiteral("稳定中"), QStringLiteral("#89b4fa"));
    QTimer::singleShot(m_settlingMs, this, &AutoCalibrationWidget::onSettlingFinished);
}

void AutoCalibrationWidget::onSettlingFinished()
{
    if (m_state != State::Settling) {
        return; // 鍙兘宸茶鍋滄/璺宠繃
    }
    launchCaptureScript();
}

void AutoCalibrationWidget::launchCaptureScript()
{
    if (m_currentIndex < 0 || m_currentIndex >= static_cast<int>(m_waypoints.size())) {
        return;
    }
    const Waypoint& wp = m_waypoints[m_currentIndex];

    m_state = State::Capturing;
    m_stateLabel->setText(QStringLiteral("状态: 采图中"));
    setRowStatus(m_currentIndex, QStringLiteral("采图中"), QStringLiteral("#89b4fa"));

    const auto& actual = isJointCoord() ? m_curJoint : m_curCart;
    const QString coordName = isJointCoord() ? QStringLiteral("joint") : QStringLiteral("cartesian");

    const QStringList args = {
        m_captureScriptPath,
        QStringLiteral("--pose-index"), QString::number(m_currentIndex),
        QStringLiteral("--pose-id"), QString::number(wp.id),
        QStringLiteral("--pose-name"), wp.name,
        QStringLiteral("--target-pose"), poseToCsv(wp.pose),
        QStringLiteral("--actual-pose"), poseToCsv(actual),
        QStringLiteral("--coord"), coordName,
        QStringLiteral("--output-dir"), m_sessionDir,
        QStringLiteral("--session-id"), m_sessionId
    };
    appendSessionLog(QStringLiteral("启动采图: %1 %2")
                         .arg(m_pythonExe, args.join(' ')));
    m_captureProcess->setWorkingDirectory(m_sessionDir);
    m_captureProcess->start(m_pythonExe, args);
    if (!m_captureProcess->waitForStarted(3000)) {
        const QString msg = QStringLiteral("采图脚本无法启动（请检查 Python 和脚本路径）");
        QJsonObject info;
        info["accepted"] = false;
        info["reason"] = QStringLiteral("capture_process_failed_to_start");
        onPoseRejected(msg, info);
    }
}

void AutoCalibrationWidget::onCaptureProcessReadStdout()
{
    const QByteArray data = m_captureProcess->readAllStandardOutput();
    if (data.isEmpty()) return;
    const QString text = QString::fromLocal8Bit(data);
    appendSessionLog(QStringLiteral("<pre style='margin:0'>%1</pre>")
                         .arg(text.toHtmlEscaped()));
}

void AutoCalibrationWidget::onCaptureProcessReadStderr()
{
    const QByteArray data = m_captureProcess->readAllStandardError();
    if (data.isEmpty()) return;
    const QString text = QString::fromLocal8Bit(data);
    appendSessionLog(colorTag(QStringLiteral("#f38ba8"),
                              QStringLiteral("<pre style='margin:0'>%1</pre>").arg(text.toHtmlEscaped())));
}

void AutoCalibrationWidget::onCaptureProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    if (m_manualCaptureActive) {
        m_manualCaptureActive = false;
        m_state = State::Idle;
        m_stateLabel->setText(status == QProcess::NormalExit && exitCode == 0
                                  ? QStringLiteral("状态: 采图验证完成")
                                  : QStringLiteral("状态: 采图验证失败"));
        appendSessionLog(colorTag(status == QProcess::NormalExit && exitCode == 0
                                      ? QStringLiteral("#a6e3a1")
                                      : QStringLiteral("#f38ba8"),
                                  status == QProcess::NormalExit && exitCode == 0
                                      ? QStringLiteral("【采图验证完成】")
                                      : QStringLiteral("【采图验证失败】exit=%1").arg(exitCode)));
        m_manualCaptureBtn->setEnabled(true);
        m_manualCalibBtn->setEnabled(true);
        return;
    }
    if (m_state == State::Stopping) {
        // 鐢?onStopClicked 涓诲姩 kill 瑙﹀彂鐨?finished锛屽悆鎺夊嵆鍙?        return;
    }
    if (m_state != State::Capturing) {
        // 鐢辫烦杩?瓒呮椂瑙﹀彂鐨?kill锛歰nPoseRejected 宸茬粡鎶婄姸鎬佹帹杩涘埌涓嬩竴鐐癸紝杩欓噷鍒噸澶嶈Е鍙?        return;
    }
    m_perPoseTimeoutTimer->stop();

    const QByteArray stdoutBytes = m_captureProcess->readAllStandardOutput();
    const QString stdoutText = QString::fromLocal8Bit(stdoutBytes);
    const QString lastJsonLine = readLastJsonLine(stdoutText);

    QJsonObject info;
    if (!lastJsonLine.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(lastJsonLine.toUtf8());
        if (doc.isObject()) {
            info = doc.object();
        }
    }
    if (status != QProcess::NormalExit) {
        info["accepted"] = false;
        info["reason"] = QStringLiteral("crashed");
    } else if (!info.contains("accepted")) {
        info["accepted"] = (exitCode == 0);
        if (exitCode != 0 && !info.contains("reason")) {
            info["reason"] = QStringLiteral("exit_code_%1").arg(exitCode);
        }
    }
    info["exit_code"] = exitCode;

    const bool accepted = info.value(QStringLiteral("accepted")).toBool(false);
    if (accepted) {
        onPoseAccepted(info);
    } else {
        const QString reason = info.value(QStringLiteral("reason")).toString(QStringLiteral("rejected"));
        onPoseRejected(reason, info);
    }
}

void AutoCalibrationWidget::onPerPoseTimeout()
{
    if (m_state == State::Idle || m_state == State::FinishedAll) {
        return;
    }
    appendSessionLog(colorTag(QStringLiteral("#f38ba8"),
                              QStringLiteral("第 %1 个路径点超时（>%2 ms），已跳过。")
                                  .arg(m_currentIndex + 1).arg(m_perPoseTimeoutMs)));
    if (m_captureProcess->state() != QProcess::NotRunning) {
        m_captureProcess->kill();
    }
    m_motionPollTimer->stop();

    QJsonObject info;
    info["accepted"] = false;
    info["reason"] = QStringLiteral("per_pose_timeout");
    onPoseRejected(QStringLiteral("瓒呮椂"), info);
}

void AutoCalibrationWidget::onPoseAccepted(const QJsonObject& info)
{
    const Waypoint& wp = m_waypoints[m_currentIndex];
    const auto& actual = isJointCoord() ? m_curJoint : m_curCart;
    setRowStatus(m_currentIndex, QStringLiteral("通过"), QStringLiteral("#a6e3a1"));
    QString detail;
    if (info.contains("num_corners")) {
        detail += QStringLiteral("角点 %1 ").arg(info.value("num_corners").toInt());
    }
    if (info.contains("sharpness")) {
        detail += QStringLiteral("清晰度 %1 ").arg(info.value("sharpness").toDouble(), 0, 'f', 1);
    }
    if (info.contains("image_path")) {
        detail += QStringLiteral("图像 %1").arg(info.value("image_path").toString());
    }
    setRowDetail(m_currentIndex, detail);
    appendSessionLog(colorTag(QStringLiteral("#a6e3a1"),
                              QStringLiteral("[%1/%2] 通过 %3 %4")
                                  .arg(m_currentIndex + 1)
                                  .arg(m_waypoints.size())
                                  .arg(wp.name, detail)));

    QJsonObject record;
    record["index"] = m_currentIndex;
    record["id"] = wp.id;
    record["name"] = wp.name;
    record["coord"] = isJointCoord() ? QStringLiteral("joint") : QStringLiteral("cartesian");
    record["target_pose"] = poseToJsonArray(wp.pose);
    record["actual_pose"] = poseToJsonArray(actual);
    record["accepted"] = true;
    record["captured_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    record["script_result"] = info;
    m_captureRecords.append(record);

    m_state = State::BetweenPoses;
    startNextPose();
}

void AutoCalibrationWidget::onPoseRejected(const QString& reason, const QJsonObject& info)
{
    if (m_currentIndex < 0 || m_currentIndex >= static_cast<int>(m_waypoints.size())) {
        return;
    }
    const Waypoint& wp = m_waypoints[m_currentIndex];
    const auto& actual = isJointCoord() ? m_curJoint : m_curCart;
    setRowStatus(m_currentIndex, QStringLiteral("不通过"), QStringLiteral("#f38ba8"));
    setRowDetail(m_currentIndex, reason);
    appendSessionLog(colorTag(QStringLiteral("#f38ba8"),
                              QStringLiteral("[%1/%2] 不通过 %3：%4")
                                  .arg(m_currentIndex + 1)
                                  .arg(m_waypoints.size())
                                  .arg(wp.name, reason)));

    QJsonObject record;
    record["index"] = m_currentIndex;
    record["id"] = wp.id;
    record["name"] = wp.name;
    record["coord"] = isJointCoord() ? QStringLiteral("joint") : QStringLiteral("cartesian");
    record["target_pose"] = poseToJsonArray(wp.pose);
    record["actual_pose"] = poseToJsonArray(actual);
    record["accepted"] = false;
    record["captured_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    record["reason"] = reason;
    record["script_result"] = info;
    m_captureRecords.append(record);

    m_state = State::BetweenPoses;
    startNextPose();
}

// =============================================================================
// 鏍囧畾鑴氭湰杈撳嚭
// =============================================================================

void AutoCalibrationWidget::onCalibProcessReadStdout()
{
    const QByteArray data = m_calibProcess->readAllStandardOutput();
    if (data.isEmpty()) return;
    appendSessionLog(QStringLiteral("<pre style='margin:0'>%1</pre>")
                         .arg(QString::fromLocal8Bit(data).toHtmlEscaped()));
}

void AutoCalibrationWidget::onCalibProcessReadStderr()
{
    const QByteArray data = m_calibProcess->readAllStandardError();
    if (data.isEmpty()) return;
    appendSessionLog(colorTag(QStringLiteral("#f38ba8"),
                              QStringLiteral("<pre style='margin:0'>%1</pre>")
                                  .arg(QString::fromLocal8Bit(data).toHtmlEscaped())));
}

void AutoCalibrationWidget::onCalibProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    if (m_manualCalibActive) {
        m_manualCalibActive = false;
        m_state = State::Idle;
        m_stateLabel->setText(status == QProcess::NormalExit && exitCode == 0
                                  ? QStringLiteral("状态: 标定验证完成")
                                  : QStringLiteral("状态: 标定验证失败"));
        appendSessionLog(colorTag(status == QProcess::NormalExit && exitCode == 0
                                      ? QStringLiteral("#a6e3a1")
                                      : QStringLiteral("#f38ba8"),
                                  status == QProcess::NormalExit && exitCode == 0
                                      ? QStringLiteral("【标定验证完成】")
                                      : QStringLiteral("【标定验证失败】exit=%1").arg(exitCode)));
        m_manualCaptureBtn->setEnabled(true);
        m_manualCalibBtn->setEnabled(true);
        return;
    }
    m_state = State::Idle;
    if (status == QProcess::NormalExit && exitCode == 0) {
        appendSessionLog(colorTag(QStringLiteral("#a6e3a1"),
                                  QStringLiteral("【离线标定完成】结果已写入 %1/calibration_result.json")
                                      .arg(QDir::toNativeSeparators(m_sessionDir))));
        m_stateLabel->setText(QStringLiteral("状态: 标定完成"));
    } else {
        appendSessionLog(colorTag(QStringLiteral("#f38ba8"),
                                  QStringLiteral("【离线标定失败】exit=%1").arg(exitCode)));
        m_stateLabel->setText(QStringLiteral("状态: 标定失败"));
    }
}

// =============================================================================
// 钀界洏 / 鍏抽棴
// =============================================================================

void AutoCalibrationWidget::finalizeAndDumpCaptures()
{
    if (m_sessionDir.isEmpty()) return;

    QJsonObject root;
    root["session_id"] = m_sessionId;
    root["finalized_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["coord"] = isJointCoord() ? QStringLiteral("joint") : QStringLiteral("cartesian");
    root["move_type"] = (m_moveType == MoveType::MoveL) ? QStringLiteral("movel") : QStringLiteral("movej");
    root["robot_pose_frame"] = QStringLiteral("base->flange");
    root["config_path"] = m_configPath;
    root["capture_script"] = m_captureScriptPath;
    root["items"] = m_captureRecords;

    int accepted = 0;
    for (const QJsonValue& v : m_captureRecords) {
        if (v.toObject().value("accepted").toBool()) accepted++;
    }
    root["samples_total"] = m_captureRecords.size();
    root["samples_accepted"] = accepted;

    const QString path = QDir(m_sessionDir).absoluteFilePath(QStringLiteral("captures.json"));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        appendSessionLog(colorTag(QStringLiteral("#f38ba8"),
                                  QStringLiteral("无法写入 captures.json：%1").arg(path)));
        return;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
    appendSessionLog(QStringLiteral("已写入 captures.json: %1 (accepted=%2/%3)")
                         .arg(QDir::toNativeSeparators(path))
                         .arg(accepted).arg(m_captureRecords.size()));
}

void AutoCalibrationWidget::abortSession(const QString& reason)
{
    appendSessionLog(colorTag(QStringLiteral("#f38ba8"), reason));
    m_state = State::Idle;
    m_motionPollTimer->stop();
    m_perPoseTimeoutTimer->stop();
    if (m_captureProcess->state() != QProcess::NotRunning) {
        m_captureProcess->kill();
    }
    finalizeAndDumpCaptures();
    setControlsEnabled(false);
}

// =============================================================================
// 宸ュ叿
// =============================================================================

bool AutoCalibrationWidget::isJointCoord() const
{
    return m_coord == Coord::Joint;
}

void AutoCalibrationWidget::setRowStatus(int row, const QString& status, const QString& color)
{
    if (row < 0 || row >= m_table->rowCount()) return;
    QTableWidgetItem *item = m_table->item(row, 3);
    if (!item) {
        item = new QTableWidgetItem();
        m_table->setItem(row, 3, item);
    }
    item->setText(status);
    item->setForeground(QColor(color));
}

void AutoCalibrationWidget::setRowDetail(int row, const QString& detail)
{
    if (row < 0 || row >= m_table->rowCount()) return;
    QTableWidgetItem *item = m_table->item(row, 4);
    if (!item) {
        item = new QTableWidgetItem();
        m_table->setItem(row, 4, item);
    }
    item->setText(detail);
}

void AutoCalibrationWidget::appendSessionLog(const QString& html)
{
    const QString stamped = QStringLiteral("<span style='color:#7f849c;'>[%1]</span> %2")
                                .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), html);
    m_sessionLog->append(stamped);
    emit log(stamped);
}

QString AutoCalibrationWidget::poseToCsv(const std::array<double, 7>& pose) const
{
    QStringList parts;
    parts.reserve(kVisiblePoseAxisCount);
    for (int i = 0; i < kVisiblePoseAxisCount; ++i) {
        parts << QString::number(pose[i], 'f', 3);
    }
    return parts.join(',');
}

QJsonArray AutoCalibrationWidget::poseToJsonArray(const std::array<double, 7>& pose) const
{
    QJsonArray a;
    for (int i = 0; i < kVisiblePoseAxisCount; ++i) a.append(pose[i]);
    return a;
}

bool AutoCalibrationWidget::queryCurrentPoseForRecord(std::array<double, 7>* jointPoseOut,
                                                      std::array<double, 7>* cartPoseOut,
                                                      QString* errorOut) const
{
    if (!jointPoseOut || !cartPoseOut) {
        if (errorOut) *errorOut = QStringLiteral("位姿输出参数无效。");
        return false;
    }

    *jointPoseOut = m_curJoint;
    *cartPoseOut = m_curCart;
    (*jointPoseOut)[6] = 0.0;
    (*cartPoseOut)[6] = 0.0;

    if (!m_connected) {
        if (errorOut) *errorOut = QStringLiteral("请先连接控制器或进入演示模式后再采图。");
        return false;
    }
    if (m_demoMode) {
        return true;
    }
    if (m_socketFd == -1) {
        if (errorOut) *errorOut = QStringLiteral("控制器连接无效，无法读取当前位姿。");
        return false;
    }

    auto fillPose = [](const std::vector<double>& src, std::array<double, 7>* dst) {
        dst->fill(0.0);
        for (int i = 0; i < kVisiblePoseAxisCount && i < static_cast<int>(src.size()); ++i) {
            (*dst)[i] = src[static_cast<size_t>(i)];
        }
        (*dst)[6] = 0.0;
    };

    std::vector<double> jointPose;
    if (get_current_position(m_socketFd, 0, jointPose) == SUCCESS && jointPose.size() >= kVisiblePoseAxisCount) {
        fillPose(jointPose, jointPoseOut);
    }

    std::vector<double> cartPose;
    if (get_current_position(m_socketFd, 1, cartPose) != SUCCESS || cartPose.size() < kVisiblePoseAxisCount) {
        if (errorOut) *errorOut = QStringLiteral("读取当前末端位姿失败。");
        return false;
    }
    fillPose(cartPose, cartPoseOut);
    return true;
}

bool AutoCalibrationWidget::appendManualCaptureSnapshot(const std::array<double, 7>& jointPose,
                                                        const std::array<double, 7>& cartPose,
                                                        QString* savedPathOut)
{
    QString outputRoot = m_outputRootEdit ? m_outputRootEdit->text().trimmed() : m_outputRoot;
    if (outputRoot.isEmpty()) {
        outputRoot = defaultOutputRoot();
        if (m_outputRootEdit) {
            m_outputRootEdit->setText(outputRoot);
        }
    }

    if (!QDir().mkpath(outputRoot)) {
        return false;
    }

    const QString path = manualCaptureLogPath();
    QJsonObject root;
    QJsonArray items;

    QFile file(path);
    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        if (!doc.isNull() && doc.isObject()) {
            root = doc.object();
            items = root.value(QStringLiteral("items")).toArray();
        }
    }

    QJsonObject record;
    record["index"] = items.size() + 1;
    record["captured_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    record["pose"] = poseToJsonArray(cartPose);
    record["end_effector_pose"] = poseToJsonArray(cartPose);
    record["joint_pose"] = poseToJsonArray(jointPose);
    record["coord"] = QStringLiteral("cartesian");
    record["robot_pose_frame"] = QStringLiteral("base->flange");
    record["trigger"] = QStringLiteral("manual_capture_button");
    items.append(record);

    root["record_type"] = QStringLiteral("manual_capture");
    root["updated_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["robot_pose_frame"] = QStringLiteral("base->flange");
    root["items"] = items;
    root["samples_total"] = items.size();

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();

    if (savedPathOut) {
        *savedPathOut = path;
    }
    return true;
}

QString AutoCalibrationWidget::manualCaptureLogPath() const
{
    QString outputRoot = m_outputRootEdit ? m_outputRootEdit->text().trimmed() : m_outputRoot;
    if (outputRoot.isEmpty()) {
        outputRoot = defaultOutputRoot();
    }
    return QDir(outputRoot).absoluteFilePath(QStringLiteral("manual_capture_records.json"));
}

void AutoCalibrationWidget::setControlsEnabled(bool running)
{
    if (m_startBtn) {
        m_startBtn->setEnabled(!running);
    }
    if (m_pauseBtn) {
        m_pauseBtn->setEnabled(running);
        m_pauseBtn->setText(QStringLiteral("暂停"));
    }
    m_skipBtn->setEnabled(running);
    if (m_stopBtn) {
        m_stopBtn->setEnabled(running);
    }
    m_manualCaptureBtn->setEnabled(!running && !m_manualCaptureActive && !m_manualCalibActive);
    m_manualCalibBtn->setEnabled(!running && !m_manualCaptureActive && !m_manualCalibActive);
}

QString AutoCalibrationWidget::stateName(State s) const
{
    switch (s) {
    case State::Idle: return QStringLiteral("空闲");
    case State::Moving: return QStringLiteral("移动中");
    case State::Settling: return QStringLiteral("稳定中");
    case State::Capturing: return QStringLiteral("采图中");
    case State::BetweenPoses: return QStringLiteral("点间过渡");
    case State::FinishedAll: return QStringLiteral("已全部完成");
    case State::Calibrating: return QStringLiteral("离线标定中");
    case State::Stopping: return QStringLiteral("停止中");
    case State::Error: return QStringLiteral("错误");
    }
    return QStringLiteral("-");
}

QString AutoCalibrationWidget::sessionTimestamp() const
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss"));
}

QString AutoCalibrationWidget::currentSessionDir() const
{
    return m_sessionDir;
}

