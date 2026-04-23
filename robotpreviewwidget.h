#ifndef ROBOTPREVIEWWIDGET_H
#define ROBOTPREVIEWWIDGET_H

#include <QWidget>
#include <QMatrix4x4>
#include <QVector>
#include <QVector3D>

#include <array>

class RobotPreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RobotPreviewWidget(QWidget *parent = nullptr);

    void setRobotState(const std::array<double, 7>& jointAnglesDeg,
                       const QVector3D& tcpPositionMm,
                       bool appendTrace = true);
    void clearTrace();

    static QVector<QVector3D> buildRobotPoints(const std::array<double, 7>& jointAnglesDeg);
    static QVector3D toolPositionFromJoints(const std::array<double, 7>& jointAnglesDeg);
    static std::array<double, 7> approximateJointsFromTcp(const std::array<double, 7>& cartesianPose);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    QMatrix4x4 buildViewMatrix() const;
    QMatrix4x4 buildProjectionMatrix() const;
    QPointF projectPoint(const QVector3D& point, const QMatrix4x4& viewProjection, bool* ok) const;
    float pointDepth(const QVector3D& point, const QMatrix4x4& viewMatrix) const;
    void appendTracePoint(const QVector3D& point);

    std::array<double, 7> m_jointAngles = {};
    QVector3D m_tcpPosition = {};
    QVector<QVector3D> m_tracePoints;

    float m_cameraYaw = -140.0f;
    float m_cameraPitch = 24.0f;
    float m_cameraDistance = 1450.0f;
    QVector3D m_cameraTarget = QVector3D(160.0f, 0.0f, 180.0f);

    bool m_dragging = false;
    QPoint m_lastMousePos;
};

#endif // ROBOTPREVIEWWIDGET_H
