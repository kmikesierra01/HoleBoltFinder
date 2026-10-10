#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QMatrix4x4>
#include <QQuaternion>
#include <QPoint>
#include <QLabel>
#include "Types.h"

class GlPCViewer : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit GlPCViewer(QWidget* parent = nullptr);
    ~GlPCViewer();

public slots:
    void setPointCloud(const PCData& pc);
    void setCropBox(float cx, float cy, float cz, float sx, float sy, float sz);
    void hideCropBox();
    void focusOnBox(float cx, float cy, float cz, float sx, float sy, float sz);
    void setHoleResult(const Circle3D& circle);
    void setBoltResult(const Circle3D& circle);
    void clearResults();

signals:
    void cropCenterPicked(float x, float y, float z);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    void buildShader();
    void buildBoxShader();
    void updateBoxVbo();
    void drawCropBox();
    void buildHoleShader();
    void drawHole();
    void drawContour();
    void drawPlane();
    QMatrix4x4 buildMVP() const;
    void showCoordinates(int x, int y);
    void drawLegend(QPainter& p);
    void drawControlHints(QPainter& p);
    QVector3D pickWorld(int x, int y, bool& valid);

    QOpenGLShaderProgram     m_shader;
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer            m_vbo;
    int                      m_pointCount = 0;
    float                    m_minZ       = 0.0f;
    float                    m_maxZ       = 1.0f;

    // Crop box
    QOpenGLShaderProgram     m_boxShader;
    QOpenGLVertexArrayObject m_boxVao;
    QOpenGLBuffer            m_boxVbo;
    QVector3D                m_cropCenter;
    QVector3D                m_cropSize;
    bool                     m_showCropBox = false;

    // Hole visualization
    QOpenGLShaderProgram     m_holeShader;
    QOpenGLVertexArrayObject m_sphereVao;
    QOpenGLBuffer            m_sphereVbo;
    int                      m_sphereVertCount = 0;
    QOpenGLVertexArrayObject m_contourVao;
    QOpenGLBuffer            m_contourVbo;
    int                      m_contourVertCount = 0;
    QOpenGLVertexArrayObject m_planeVao;
    QOpenGLBuffer            m_planeVbo;
    int                      m_planeVertCount  = 0;
    QVector3D                m_planeLabelPos;  // world-space corner for "Plane" text
    bool                     m_showPlane       = false;
    Circle3D                 m_holeResult;
    bool                     m_showHole        = false;
    Circle3D                 m_boltResult;
    bool                     m_showBolt        = false;

    // Camera
    QVector3D   m_center;   // point cloud centroid (initial value)
    QVector3D   m_target;   // current rotation pivot (world space)
    float       m_zoom     = 1.0f;
    QQuaternion m_rotation = QQuaternion::fromAxisAndAngle(1.0f, 0.0f, 0.0f, 45.0f);
    QPoint      m_lastMouse;

    QLabel*   m_coordLabel = nullptr;
};
