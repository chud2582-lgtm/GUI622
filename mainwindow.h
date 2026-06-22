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
#include <QSpinBox>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>

#include <vector>

#include "robot_sdk/cpp/parameter/nrc_define.h"
#include "robot_sdk/cpp/robot_adapter.h"

class WorkpieceSelectionWidget;
class QThread;

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

    void onTopJobStart();
    void onTopJobPauseContinue();
    void onTopJobStop();
    void onTopJobReset();

    void onSetDigitalOutput();
    void onRefreshIO();

    void onSetGlobalVariant();
    void onGetGlobalVariant();
    void onToggleWindGrinder();

    void onTimerUpdate();

private:
    void setupUI();
    void setupStyleSheet();

    QWidget* createConnectionPanel();
    QWidget* createServoPanel();
    QWidget* createIOPanel();
    QWidget* createGlobalVariantPanel();
    QWidget* createWorkpieceSelectionPanel();
    QWidget* createTcpPosePanel();
    void applyGlobalVariantInputType();
    void refreshGlobalVariantModeLabel();
    bool currentGlobalVariantName(QString& nameOut) const;
    void loadConnectionSettings();
    void saveConnectionSettings() const;
    QString connectionBrandText() const;
    void applySelectedRobotBrand();
    RobotBrand selectedRobotBrand() const;

    QPushButton* createStyledButton(const QString& text, const QString& styleClass = QString());
    QGroupBox* createStyledGroup(const QString& title);

    void appendLog(const QString& msg);
    void stopTimers();
    void setConnectionUi(bool connected);
    void setConnectionInProgress(bool inProgress);
    void finishConnect(const QString& ip, const QString& port, SOCKETFD socketFd, qint64 elapsedMs,
                       const QString& failureDetail, const QString& tcpProbeDetail,
                       const QString& detectedBrand);
    void updateConnectionStatusLabel(const QString& text, bool connectedStyle);
    void refreshStatusSummary();
    void refreshTcpPoseLabels();
    void updateTopJobButtons();
    void setTopJobStatus(const QString& status, bool paused);
    bool ensureJobControllerReady(const QString& actionName);
    bool pollRobotState();
    QString normalizedJobName() const;
    QString resultToString(Result result) const;
    QString robotTypeName(int type) const;

    QLabel* m_connectionEndpointLabel = nullptr;
    QLabel* m_brandLabel = nullptr;
    QPushButton* m_connectBtn = nullptr;
    QPushButton* m_disconnectBtn = nullptr;
    QProgressBar* m_connProgress = nullptr;
    QLabel* m_connStatusLabel = nullptr;

    QPushButton* m_topJobStartBtn = nullptr;
    QPushButton* m_topJobPauseContinueBtn = nullptr;
    QPushButton* m_topJobStopBtn = nullptr;
    QPushButton* m_topJobResetBtn = nullptr;
    QPushButton* m_servoPowerOnBtn = nullptr;
    QLabel* m_topJobStatusLabel = nullptr;
    QLabel* m_servoStatusLabel = nullptr;

    QComboBox* m_modeCombo = nullptr;

    QLineEdit* m_workpieceIdEdit = nullptr;
    QLineEdit* m_jobNameEdit = nullptr;

    QSpinBox* m_doPortSpin = nullptr;
    QComboBox* m_doValueCombo = nullptr;
    QPushButton* m_doSetBtn = nullptr;
    QPushButton* m_ioRefreshBtn = nullptr;
    QTableWidget* m_diTable = nullptr;
    QTableWidget* m_doTable = nullptr;

    QLineEdit* m_globalVarNameEdit = nullptr;
    QComboBox* m_globalVarPresetCombo = nullptr;
    QDoubleSpinBox* m_globalVarValueSpin = nullptr;
    QLabel* m_globalVarTypeLabel = nullptr;
    QLabel* m_globalVarModeLabel = nullptr;
    QPushButton* m_globalVarSetBtn = nullptr;
    QPushButton* m_globalVarGetBtn = nullptr;
    QLabel* m_globalVarResultLabel = nullptr;
    QPushButton* m_windGrinderBtn = nullptr;
    bool m_windGrinderOn = false;

    WorkpieceSelectionWidget* m_workpieceSelection = nullptr;

    QLabel* m_robotRunStateLabel = nullptr;
    QLabel* m_robotTypeLabel = nullptr;
    QLabel* m_tcpPoseValueLabels[6] = {};
    QTextEdit* m_logEdit = nullptr;

    QTimer* m_updateTimer = nullptr;
    QThread* m_connectThread = nullptr;

    QString m_robotIp;
    QString m_robotPort;
    QString m_robotBrand;
    QString m_lastSuccessfulBrand;
    SOCKETFD m_socketFd = -1;
    bool m_connected = false;
    bool m_connecting = false;
    bool m_workpieceSelected = false;
    bool m_topJobPaused = false;

    int m_servoState = 0;
    int m_runState = 0;
    int m_robotType = 12;
    int m_pollFailCount = 0;
    int m_pollSequence = 0;
    std::vector<double> m_tcpPose;
};

#endif // MAINWINDOW_H
