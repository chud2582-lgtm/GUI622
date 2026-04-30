#ifndef AUTOCALIBRATIONWIDGET_H
#define AUTOCALIBRATIONWIDGET_H

#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QWidget>

#include <array>
#include <vector>

#include "cpp/parameter/nrc_define.h"

class QComboBox;
class QGroupBox;

class AutoCalibrationWidget : public QWidget
{
    Q_OBJECT

public:
    enum class Coord { Joint = 0, Cartesian = 1 };
    enum class MoveType { MoveJ = 0, MoveL = 1 };

    enum class State {
        Idle,
        Moving,        // movej/movel 已下发，等控制器把动作做完
        Settling,      // 已到位，进入稳定窗口
        Capturing,     // python 采图脚本运行中
        BetweenPoses,  // 上一点完成，准备进入下一点
        FinishedAll,   // 60 个全部完成
        Calibrating,   // 离线标定脚本运行中
        Stopping,
        Error
    };

    explicit AutoCalibrationWidget(QWidget *parent = nullptr);

public slots:
    // 由 MainWindow 在 connect/disconnect 时调用
    void setConnectionState(bool connected, bool demoMode, SOCKETFD socketFd);
    // 由 MainWindow 在每次 pollRobotState 后调用
    void updateRobotState(const std::array<double, 7>& joint,
                          const std::array<double, 7>& cartesian,
                          int runState);

signals:
    // 转发到 MainWindow::appendLog
    void log(const QString& msg);

private slots:
    void onBrowseWaypoints();
    void onReloadWaypoints();
    void onBrowseCaptureScript();
    void onBrowseCalibScript();
    void onBrowseOutputDir();
    void onBrowsePython();
    void onTestPython();
    void onManualCaptureClicked();
    void onManualCalibClicked();
    void onStartClicked();
    void onPauseClicked();
    void onSkipClicked();
    void onStopClicked();
    void onLaunchCalibrationClicked();

    void onMotionPollTick();
    void onSettlingFinished();
    void onCaptureProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onCaptureProcessReadStdout();
    void onCaptureProcessReadStderr();
    void onCalibProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onCalibProcessReadStdout();
    void onCalibProcessReadStderr();
    void onPerPoseTimeout();

private:
    struct Waypoint {
        int id = 0;
        QString name;
        std::array<double, 7> pose = {};
        // 单点可覆盖速度（>0 时生效）
        double velocity = 0.0;
    };

    // === UI 构建 ===
    void buildUi();
    QGroupBox* buildConfigGroup();
    QGroupBox* buildMotionGroup();
    QGroupBox* buildScriptGroup();
    QGroupBox* buildControlGroup();
    QGroupBox* buildTableGroup();
    QGroupBox* buildLogGroup();

    // === 配置加载 / 默认路径 ===
    QString defaultWaypointsPath() const;
    QString defaultCaptureScriptPath() const;
    QString defaultCalibScriptPath() const;
    QString defaultOutputRoot() const;
    QString verificationCaptureScriptPath() const;
    QString verificationCalibScriptPath() const;
    void loadPathSelections();
    void savePathSelections() const;
    bool loadWaypoints(const QString& path, QString* errorOut);
    void rebuildTableFromWaypoints();

    // === 状态机推动 ===
    void startNextPose();
    void issueMoveCommand(const Waypoint& wp);
    void enterSettling();
    void launchCaptureScript();
    void onPoseAccepted(const QJsonObject& info);
    void onPoseRejected(const QString& reason, const QJsonObject& info);
    void finalizeAndDumpCaptures();
    void abortSession(const QString& reason);

    // === 工具 ===
    bool isJointCoord() const;
    bool poseReached(const Waypoint& wp) const;
    void setRowStatus(int row, const QString& status, const QString& color);
    void setRowDetail(int row, const QString& detail);
    void appendSessionLog(const QString& html);
    QString poseToCsv(const std::array<double, 7>& pose) const;
    QJsonArray poseToJsonArray(const std::array<double, 7>& pose) const;
    bool queryCurrentPoseForRecord(std::array<double, 7>* jointPoseOut,
                                   std::array<double, 7>* cartPoseOut,
                                   QString* errorOut = nullptr) const;
    bool appendManualCaptureSnapshot(const std::array<double, 7>& jointPose,
                                     const std::array<double, 7>& cartPose,
                                     QString* savedPathOut = nullptr);
    QString manualCaptureLogPath() const;
    void setControlsEnabled(bool running);
    bool startVerificationProcess(QProcess* process,
                                  const QString& scriptPath,
                                  const QString& actionName,
                                  const QString& stateText,
                                  bool isCapture);
    QString stateName(State s) const;
    QString sessionTimestamp() const;
    QString currentSessionDir() const;

    // === 配置项 ===
    QString m_configPath;
    QString m_captureScriptPath;
    QString m_calibScriptPath;
    QString m_pythonExe = QStringLiteral("python");
    QString m_outputRoot;
    QString m_sessionId;
    QString m_sessionDir;

    Coord m_coord = Coord::Joint;
    MoveType m_moveType = MoveType::MoveJ;
    int m_velocity = 25;
    int m_acceleration = 40;
    int m_deceleration = 40;
    int m_settlingMs = 400;
    int m_perPoseTimeoutMs = 30000;
    double m_jointTolDeg = 0.5;
    double m_linearTolMm = 1.0;
    double m_angularTolDeg = 0.5;

    std::vector<Waypoint> m_waypoints;
    int m_currentIndex = -1;
    bool m_pauseRequested = false;

    // === 运行时连接状态（由 MainWindow 同步进来） ===
    bool m_connected = false;
    bool m_demoMode = false;
    SOCKETFD m_socketFd = -1;
    std::array<double, 7> m_curJoint = {};
    std::array<double, 7> m_curCart = {};
    int m_runState = 0;

    State m_state = State::Idle;
    qint64 m_motionStartedAtMs = 0;

    // === 每帧采集结果累计（最终写入 captures.json） ===
    QJsonArray m_captureRecords;

    // === UI ===
    QLineEdit* m_configPathEdit = nullptr;
    QPushButton* m_browseConfigBtn = nullptr;
    QPushButton* m_reloadConfigBtn = nullptr;

    QLineEdit* m_captureScriptEdit = nullptr;
    QPushButton* m_browseCaptureBtn = nullptr;
    QLineEdit* m_calibScriptEdit = nullptr;
    QPushButton* m_browseCalibBtn = nullptr;
    QLineEdit* m_pythonExeEdit = nullptr;
    QPushButton* m_browsePythonBtn = nullptr;
    QPushButton* m_testPythonBtn = nullptr;
    QLineEdit* m_outputRootEdit = nullptr;
    QPushButton* m_browseOutputBtn = nullptr;

    QComboBox* m_coordCombo = nullptr;
    QComboBox* m_moveTypeCombo = nullptr;
    QSpinBox* m_velocitySpin = nullptr;
    QSpinBox* m_accSpin = nullptr;
    QSpinBox* m_decSpin = nullptr;
    QSpinBox* m_settlingSpin = nullptr;
    QSpinBox* m_perPoseTimeoutSpin = nullptr;

    QPushButton* m_startBtn = nullptr;
    QPushButton* m_pauseBtn = nullptr;
    QPushButton* m_skipBtn = nullptr;
    QPushButton* m_stopBtn = nullptr;
    QPushButton* m_manualCaptureBtn = nullptr;
    QPushButton* m_manualCalibBtn = nullptr;
    QPushButton* m_launchCalibBtn = nullptr;
    QLabel* m_stateLabel = nullptr;
    QLabel* m_progressLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;

    QTableWidget* m_table = nullptr;
    QTextEdit* m_sessionLog = nullptr;

    // === 计时器 / 进程 ===
    QTimer* m_motionPollTimer = nullptr;
    QTimer* m_perPoseTimeoutTimer = nullptr;
    QProcess* m_captureProcess = nullptr;
    QProcess* m_calibProcess = nullptr;
    bool m_manualCaptureActive = false;
    bool m_manualCalibActive = false;
};

#endif // AUTOCALIBRATIONWIDGET_H
