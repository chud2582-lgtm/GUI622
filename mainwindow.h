#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVector3D>

#include <array>

#include "cpp/parameter/nrc_define.h"
#include "cpp/parameter/nrc_interface_parameter.h"

class RobotPreviewWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onConnectClicked();
    void onDisconnectClicked();

    void onServoReady();
    void onServoPowerOn();
    void onServoPowerOff();
    void onClearError();

    void onModeChanged(int index);
    void onCoordChanged(int index);

    void onSpeedSliderChanged(int value);
    void onSpeedApply();

    void onJogStart();
    void onJogStop();

    void onMoveJ();
    void onMoveL();
    void onGoHome();
    void onGoReset();

    void onJobRun();
    void onJobPause();
    void onJobContinue();
    void onJobStop();
    void onJobCreate();
    void onJobOpen();
    void onJobDelete();

    void onSetDigitalOutput();
    void onRefreshIO();

    void onTimerUpdate();
    void onPreviewFrame();
    void onClearPreviewTrace();

private:
    enum class PreviewMotionMode {
        None,
        JointTarget,
        LinearTarget
    };

    void setupUI();
    void setupStyleSheet();

    QWidget* createConnectionPanel();
    QWidget* createServoPanel();
    QWidget* createPositionPanel();
    QWidget* createPreviewPanel();
    QWidget* createJogPanel();
    QWidget* createMotionPanel();
    QWidget* createJobPanel();
    QWidget* createIOPanel();

    QPushButton* createStyledButton(const QString& text, const QString& styleClass = QString());
    QGroupBox* createStyledGroup(const QString& title);

    void appendLog(const QString& msg);
    void stopTimers();
    void setConnectionUi(bool connected);
    void updateConnectionStatusLabel(const QString& text, bool connectedStyle);
    void refreshPositionHeaders();
    void refreshPositionDisplay();
    void refreshPreview(bool appendTrace = true);
    void refreshJobStatus();
    void refreshStatusSummary();
    void resetDemoState();
    void updateCartesianFromJointState();
    void updateJointFromCartesianState();
    void startJointAnimation(const std::array<double, 7>& joints, int velocityPercent);
    void startLinearAnimation(const std::array<double, 7>& cartesianPose, int velocity);
    void stepDemoMotion();
    bool ensureJobControllerReady(const QString& actionName);
    bool pollRobotState();
    MoveCmd buildMoveCommand() const;
    QString normalizedJobName() const;
    QString resultToString(Result result) const;
    QString robotTypeName(int type) const;
    std::array<double, 7> targetPoseFromInputs() const;
    static QVector3D poseToVector3D(const std::array<double, 7>& pose);

    QLineEdit* m_ipEdit = nullptr;
    QLineEdit* m_portEdit = nullptr;
    QPushButton* m_connectBtn = nullptr;
    QPushButton* m_disconnectBtn = nullptr;
    QLabel* m_connStatusLabel = nullptr;

    QPushButton* m_servoReadyBtn = nullptr;
    QPushButton* m_powerOnBtn = nullptr;
    QPushButton* m_powerOffBtn = nullptr;
    QPushButton* m_clearErrorBtn = nullptr;
    QLabel* m_servoStatusLabel = nullptr;

    QComboBox* m_modeCombo = nullptr;
    QComboBox* m_coordCombo = nullptr;

    QSlider* m_speedSlider = nullptr;
    QSpinBox* m_speedSpin = nullptr;
    QPushButton* m_speedApplyBtn = nullptr;

    QLabel* m_posLabels[7] = {};
    QLabel* m_posHeaderLabels[7] = {};

    RobotPreviewWidget* m_robotPreview = nullptr;
    QLabel* m_previewPoseLabel = nullptr;
    QPushButton* m_clearTraceBtn = nullptr;

    QPushButton* m_jogPosBtn[7] = {};
    QPushButton* m_jogNegBtn[7] = {};

    QDoubleSpinBox* m_targetPos[7] = {};
    QSpinBox* m_moveVelSpin = nullptr;
    QSpinBox* m_moveAccSpin = nullptr;
    QSpinBox* m_moveDecSpin = nullptr;
    QPushButton* m_moveJBtn = nullptr;
    QPushButton* m_moveLBtn = nullptr;
    QPushButton* m_goHomeBtn = nullptr;
    QPushButton* m_goResetBtn = nullptr;

    QLineEdit* m_jobNameEdit = nullptr;
    QPushButton* m_jobRunBtn = nullptr;
    QPushButton* m_jobPauseBtn = nullptr;
    QPushButton* m_jobContinueBtn = nullptr;
    QPushButton* m_jobStopBtn = nullptr;
    QPushButton* m_jobCreateBtn = nullptr;
    QPushButton* m_jobOpenBtn = nullptr;
    QPushButton* m_jobDeleteBtn = nullptr;
    QSpinBox* m_jobRunTimesSpin = nullptr;
    QLabel* m_jobCurrentLabel = nullptr;
    QLabel* m_jobLineLabel = nullptr;

    QSpinBox* m_doPortSpin = nullptr;
    QComboBox* m_doValueCombo = nullptr;
    QPushButton* m_doSetBtn = nullptr;
    QPushButton* m_ioRefreshBtn = nullptr;
    QTableWidget* m_diTable = nullptr;
    QTableWidget* m_doTable = nullptr;

    QLabel* m_robotRunStateLabel = nullptr;
    QLabel* m_robotTypeLabel = nullptr;
    QProgressBar* m_connProgress = nullptr;
    QTextEdit* m_logEdit = nullptr;

    QTimer* m_updateTimer = nullptr;
    QTimer* m_previewTimer = nullptr;

    SOCKETFD m_socketFd = -1;
    bool m_connected = false;
    bool m_demoMode = false;

    std::array<double, 7> m_jointPose = {};
    std::array<double, 7> m_cartesianPose = {};
    std::array<double, 7> m_jointTargetPose = {};
    std::array<double, 7> m_cartesianTargetPose = {};

    PreviewMotionMode m_previewMotionMode = PreviewMotionMode::None;
    int m_motionVelocity = 30;
    int m_activeJogAxis = -1;
    int m_activeJogDirection = 0;

    int m_servoState = 0;
    int m_runState = 0;
    int m_robotType = 12;
};

#endif // MAINWINDOW_H
