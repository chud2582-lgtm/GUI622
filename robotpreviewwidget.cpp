#include "robotpreviewwidget.h"

#include <QLinearGradient>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <QFont>
#include <QVector4D>
#include <QWheelEvent>

#include <QtMath>

#include <algorithm>

namespace {

constexpr float kBaseHeight = 130.0f;
constexpr float kUpperArm = 260.0f;
constexpr float kForearm = 210.0f;
constexpr float kWrist = 110.0f;
constexpr float kFlange = 65.0f;
constexpr float kTool = 80.0f;
constexpr int kMaxTracePoints = 2400;

QVector3D rotateX(const QVector3D& value, float degrees)
{
    const float radians = qDegreesToRadians(degrees);
    const float cosValue = qCos(radians);
    const float sinValue = qSin(radians);
    return QVector3D(value.x(),
                     value.y() * cosValue - value.z() * sinValue,
                     value.y() * sinValue + value.z() * cosValue);
}

QVector3D rotateY(const QVector3D& value, float degrees)
{
    const float radians = qDegreesToRadians(degrees);
    const float cosValue = qCos(radians);
    const float sinValue = qSin(radians);
    return QVector3D(value.x() * cosValue + value.z() * sinValue,
                     value.y(),
                     -value.x() * sinValue + value.z() * cosValue);
}

QVector3D rotateZ(const QVector3D& value, float degrees)
{
    const float radians = qDegreesToRadians(degrees);
    const float cosValue = qCos(radians);
    const float sinValue = qSin(radians);
    return QVector3D(value.x() * cosValue - value.y() * sinValue,
                     value.x() * sinValue + value.y() * cosValue,
                     value.z());
}

QVector3D radialDirection(float yawDeg, float pitchDeg)
{
    return rotateZ(rotateY(QVector3D(1.0f, 0.0f, 0.0f), -pitchDeg), yawDeg);
}

} // namespace

RobotPreviewWidget::RobotPreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(340);
    setMinimumSize(420, 320);
    setMouseTracking(true);
}

void RobotPreviewWidget::setRobotState(const std::array<double, 7>& jointAnglesDeg,
                                       const QVector3D& tcpPositionMm,
                                       bool appendTrace)
{
    m_jointAngles = jointAnglesDeg;
    m_tcpPosition = tcpPositionMm;

    if (appendTrace) {
        appendTracePoint(tcpPositionMm);
    }

    update();
}

void RobotPreviewWidget::clearTrace()
{
    m_tracePoints.clear();
    update();
}

QVector<QVector3D> RobotPreviewWidget::buildRobotPoints(const std::array<double, 7>& jointAnglesDeg)
{
    QVector<QVector3D> points;
    points.reserve(7);

    QVector3D current(0.0f, 0.0f, 0.0f);
    points.push_back(current);

    current.setZ(kBaseHeight);
    points.push_back(current);

    const float yaw = static_cast<float>(jointAnglesDeg[0]);
    const float shoulder = static_cast<float>(jointAnglesDeg[1]);
    const float elbow = shoulder + static_cast<float>(jointAnglesDeg[2]);
    const float wristPitch = elbow + static_cast<float>(jointAnglesDeg[3]) * 0.85f;
    const float toolPitch = wristPitch + static_cast<float>(jointAnglesDeg[5]) * 0.55f - 18.0f;
    const float wristRoll = static_cast<float>(jointAnglesDeg[4]);
    const float toolRoll = static_cast<float>(jointAnglesDeg[6]);

    current += radialDirection(yaw, shoulder) * kUpperArm;
    points.push_back(current);

    current += radialDirection(yaw, elbow) * kForearm;
    points.push_back(current);

    QVector3D wristDirection = radialDirection(yaw, wristPitch);
    wristDirection = rotateX(wristDirection, wristRoll * 0.22f);
    current += wristDirection * kWrist;
    points.push_back(current);

    QVector3D flangeDirection = radialDirection(yaw, toolPitch);
    flangeDirection = rotateX(flangeDirection, wristRoll * 0.45f + toolRoll * 0.1f);
    current += flangeDirection * kFlange;
    points.push_back(current);

    QVector3D toolDirection = radialDirection(yaw, toolPitch);
    toolDirection = rotateX(toolDirection, wristRoll * 0.8f + toolRoll);
    current += toolDirection * kTool;
    points.push_back(current);

    return points;
}

QVector3D RobotPreviewWidget::toolPositionFromJoints(const std::array<double, 7>& jointAnglesDeg)
{
    const QVector<QVector3D> points = buildRobotPoints(jointAnglesDeg);
    return points.isEmpty() ? QVector3D() : points.back();
}

std::array<double, 7> RobotPreviewWidget::approximateJointsFromTcp(const std::array<double, 7>& cartesianPose)
{
    std::array<double, 7> joints = {};

    const float x = static_cast<float>(cartesianPose[0]);
    const float y = static_cast<float>(cartesianPose[1]);
    const float z = static_cast<float>(cartesianPose[2]);

    float radius = qSqrt(x * x + y * y);
    float zRelative = z - kBaseHeight;
    const float wristReach = kWrist + kFlange + kTool * 0.85f;
    radius = qMax(50.0f, radius - wristReach);

    const float distance = qSqrt(radius * radius + zRelative * zRelative);
    const float clampedDistance = qBound(60.0f, distance, kUpperArm + kForearm - 5.0f);
    const float cosElbow = qBound(-1.0f,
                                  (clampedDistance * clampedDistance - kUpperArm * kUpperArm - kForearm * kForearm)
                                      / (2.0f * kUpperArm * kForearm),
                                  1.0f);
    const float elbow = qAcos(cosElbow);
    const float shoulder = qAtan2(zRelative, radius)
        - qAtan2(kForearm * qSin(elbow), kUpperArm + kForearm * qCos(elbow));

    joints[0] = qRadiansToDegrees(qAtan2(y, x));
    joints[1] = qRadiansToDegrees(shoulder);
    joints[2] = qRadiansToDegrees(elbow);
    joints[3] = -(joints[1] + joints[2]) * 0.70 + cartesianPose[4] * 0.18;
    joints[4] = cartesianPose[3] * 0.20;
    joints[5] = -18.0 + cartesianPose[5] * 0.15;
    joints[6] = cartesianPose[6];
    return joints;
}

void RobotPreviewWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QLinearGradient background(rect().topLeft(), rect().bottomRight());
    background.setColorAt(0.0, QColor(17, 17, 27));
    background.setColorAt(0.55, QColor(24, 24, 37));
    background.setColorAt(1.0, QColor(30, 30, 46));
    painter.fillRect(rect(), background);

    painter.setPen(QPen(QColor(69, 71, 90), 1.0));
    painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 8, 8);

    const QMatrix4x4 view = buildViewMatrix();
    const QMatrix4x4 projection = buildProjectionMatrix();
    const QMatrix4x4 viewProjection = projection * view;

    struct DrawLine {
        QVector3D a;
        QVector3D b;
        QColor color;
        qreal width;
        qreal depth;
    };

    QVector<DrawLine> gridLines;
    for (int axis = -700; axis <= 700; axis += 100) {
        const QColor gridColor = (axis == 0) ? QColor(80, 82, 102, 170) : QColor(49, 50, 68, 140);
        gridLines.push_back({QVector3D(static_cast<float>(axis), -700.0f, 0.0f),
                             QVector3D(static_cast<float>(axis), 700.0f, 0.0f),
                             gridColor,
                             axis == 0 ? 1.4 : 1.0,
                             pointDepth(QVector3D(static_cast<float>(axis), 0.0f, 0.0f), view)});
        gridLines.push_back({QVector3D(-700.0f, static_cast<float>(axis), 0.0f),
                             QVector3D(700.0f, static_cast<float>(axis), 0.0f),
                             gridColor,
                             axis == 0 ? 1.4 : 1.0,
                             pointDepth(QVector3D(0.0f, static_cast<float>(axis), 0.0f), view)});
    }

    std::sort(gridLines.begin(), gridLines.end(), [](const DrawLine& lhs, const DrawLine& rhs) {
        return lhs.depth > rhs.depth;
    });

    for (const DrawLine& line : gridLines) {
        bool okA = false;
        bool okB = false;
        const QPointF a = projectPoint(line.a, viewProjection, &okA);
        const QPointF b = projectPoint(line.b, viewProjection, &okB);
        if (!okA || !okB) {
            continue;
        }

        painter.setPen(QPen(line.color, line.width));
        painter.drawLine(a, b);
    }

    const QVector<QVector3D> robotPoints = buildRobotPoints(m_jointAngles);

    if (m_tracePoints.size() > 1) {
        painter.setPen(QPen(QColor(250, 179, 135, 210), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        for (int index = 1; index < m_tracePoints.size(); ++index) {
            bool okA = false;
            bool okB = false;
            const QPointF a = projectPoint(m_tracePoints[index - 1], viewProjection, &okA);
            const QPointF b = projectPoint(m_tracePoints[index], viewProjection, &okB);
            if (okA && okB) {
                painter.drawLine(a, b);
            }
        }
    }

    QVector<DrawLine> robotLines;
    for (int index = 1; index < robotPoints.size(); ++index) {
        const qreal depth = pointDepth((robotPoints[index - 1] + robotPoints[index]) * 0.5f, view);
        const QColor linkColor = (index >= robotPoints.size() - 2) ? QColor(249, 226, 175) : QColor(137, 180, 250);
        const qreal width = (index == 1) ? 10.0 : (index <= 3 ? 8.0 : 6.0);
        robotLines.push_back({robotPoints[index - 1], robotPoints[index], linkColor, width, depth});
    }

    std::sort(robotLines.begin(), robotLines.end(), [](const DrawLine& lhs, const DrawLine& rhs) {
        return lhs.depth > rhs.depth;
    });

    for (const DrawLine& line : robotLines) {
        bool okA = false;
        bool okB = false;
        const QPointF a = projectPoint(line.a, viewProjection, &okA);
        const QPointF b = projectPoint(line.b, viewProjection, &okB);
        if (!okA || !okB) {
            continue;
        }

        painter.setPen(QPen(QColor(17, 17, 27, 150), line.width + 4.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(a, b);
        painter.setPen(QPen(line.color, line.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(a, b);
    }

    struct DrawPoint {
        QVector3D position;
        QColor color;
        qreal radius;
        qreal depth;
    };

    QVector<DrawPoint> robotJoints;
    for (int index = 0; index < robotPoints.size(); ++index) {
        const qreal radius = (index == robotPoints.size() - 1) ? 7.5 : (index <= 1 ? 7.0 : 5.5);
        const QColor color = (index == robotPoints.size() - 1) ? QColor(166, 227, 161) : QColor(205, 214, 244);
        robotJoints.push_back({robotPoints[index], color, radius, pointDepth(robotPoints[index], view)});
    }

    std::sort(robotJoints.begin(), robotJoints.end(), [](const DrawPoint& lhs, const DrawPoint& rhs) {
        return lhs.depth > rhs.depth;
    });

    for (const DrawPoint& point : robotJoints) {
        bool ok = false;
        const QPointF projected = projectPoint(point.position, viewProjection, &ok);
        if (!ok) {
            continue;
        }

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(17, 17, 27, 180));
        painter.drawEllipse(projected, point.radius + 3.0, point.radius + 3.0);
        painter.setBrush(point.color);
        painter.drawEllipse(projected, point.radius, point.radius);
    }

    const QVector3D origin(0.0f, 0.0f, 0.0f);
    const QVector3D xAxis(260.0f, 0.0f, 0.0f);
    const QVector3D yAxis(0.0f, 260.0f, 0.0f);
    const QVector3D zAxis(0.0f, 0.0f, 260.0f);

    const struct {
        QVector3D endPoint;
        QColor color;
        QString label;
    } axes[] = {
        {xAxis, QColor(243, 139, 168), QStringLiteral("X")},
        {yAxis, QColor(166, 227, 161), QStringLiteral("Y")},
        {zAxis, QColor(137, 180, 250), QStringLiteral("Z")}
    };

    painter.setFont(QFont(QStringLiteral("Microsoft YaHei"), 10, QFont::Bold));
    for (const auto& axis : axes) {
        bool okOrigin = false;
        bool okEnd = false;
        const QPointF a = projectPoint(origin, viewProjection, &okOrigin);
        const QPointF b = projectPoint(axis.endPoint, viewProjection, &okEnd);
        if (!okOrigin || !okEnd) {
            continue;
        }

        painter.setPen(QPen(axis.color, 2.4, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(a, b);
        painter.setPen(axis.color);
        painter.drawText(b + QPointF(6.0, -6.0), axis.label);
    }

    painter.setPen(QColor(186, 194, 222));
    painter.setFont(QFont(QStringLiteral("Microsoft YaHei"), 9));
    painter.drawText(QRect(14, height() - 34, width() - 28, 20),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("拖拽旋转，滚轮缩放，双击复位视角"));
}

void RobotPreviewWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_lastMousePos = event->pos();
    }

    QWidget::mousePressEvent(event);
}

void RobotPreviewWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        const QPoint delta = event->pos() - m_lastMousePos;
        m_lastMousePos = event->pos();

        m_cameraYaw += delta.x() * 0.55f;
        m_cameraPitch = qBound(-75.0f, m_cameraPitch - delta.y() * 0.45f, 85.0f);
        update();
    }

    QWidget::mouseMoveEvent(event);
}

void RobotPreviewWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
    }

    QWidget::mouseReleaseEvent(event);
}

void RobotPreviewWidget::wheelEvent(QWheelEvent *event)
{
    const qreal steps = static_cast<qreal>(event->angleDelta().y()) / 120.0;
    m_cameraDistance = qBound(650.0f, m_cameraDistance - static_cast<float>(steps * 70.0), 2400.0f);
    update();
    event->accept();
}

void RobotPreviewWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    m_cameraYaw = -140.0f;
    m_cameraPitch = 24.0f;
    m_cameraDistance = 1450.0f;
    m_cameraTarget = QVector3D(160.0f, 0.0f, 180.0f);
    update();
}

QMatrix4x4 RobotPreviewWidget::buildViewMatrix() const
{
    const float yawRadians = qDegreesToRadians(m_cameraYaw);
    const float pitchRadians = qDegreesToRadians(m_cameraPitch);
    const float horizontal = m_cameraDistance * qCos(pitchRadians);

    const QVector3D eye(
        m_cameraTarget.x() + horizontal * qCos(yawRadians),
        m_cameraTarget.y() + horizontal * qSin(yawRadians),
        m_cameraTarget.z() + m_cameraDistance * qSin(pitchRadians));

    QMatrix4x4 view;
    view.lookAt(eye, m_cameraTarget, QVector3D(0.0f, 0.0f, 1.0f));
    return view;
}

QMatrix4x4 RobotPreviewWidget::buildProjectionMatrix() const
{
    QMatrix4x4 projection;
    projection.perspective(34.0f, static_cast<float>(width()) / qMax(1, height()), 10.0f, 5000.0f);
    return projection;
}

QPointF RobotPreviewWidget::projectPoint(const QVector3D& point,
                                         const QMatrix4x4& viewProjection,
                                         bool* ok) const
{
    const QVector4D clip = viewProjection * QVector4D(point, 1.0f);
    if (clip.w() <= 0.0f) {
        *ok = false;
        return {};
    }

    const float x = clip.x() / clip.w();
    const float y = clip.y() / clip.w();

    *ok = qAbs(x) < 4.0f && qAbs(y) < 4.0f;
    return QPointF((x * 0.5f + 0.5f) * width(), (-y * 0.5f + 0.5f) * height());
}

float RobotPreviewWidget::pointDepth(const QVector3D& point, const QMatrix4x4& viewMatrix) const
{
    return -viewMatrix.map(point).z();
}

void RobotPreviewWidget::appendTracePoint(const QVector3D& point)
{
    if (!m_tracePoints.isEmpty()) {
        const QVector3D delta = point - m_tracePoints.back();
        if (delta.lengthSquared() < 4.0f) {
            return;
        }
    }

    m_tracePoints.push_back(point);
    if (m_tracePoints.size() > kMaxTracePoints) {
        m_tracePoints.remove(0, m_tracePoints.size() - kMaxTracePoints);
    }
}
