#include "pch.h"
#include "GlPCViewer.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QPainter>
#include <QPainterPath>
#include <limits>
#include <cmath>
#include <algorithm>

static const char* BOX_VERT_SRC = R"(
#version 130
in vec3 aPos;
uniform mat4 uMVP;
void main() { gl_Position = uMVP * vec4(aPos, 1.0); }
)";

static const char* BOX_FRAG_SRC = R"(
#version 130
void main() { gl_FragColor = vec4(1.0, 0.8, 0.0, 1.0); }  // yellow wireframe
)";

static const char* VERT_SRC = R"(
#version 130
in vec3 aPos;
uniform mat4 uMVP;
out float vZ;
void main() {
    vZ = aPos.z;
    gl_Position = uMVP * vec4(aPos, 1.0);
    gl_PointSize = 2.0;
}
)";

static const char* FRAG_SRC = R"(
#version 130
in float vZ;
uniform float uMinZ;
uniform float uMaxZ;

vec3 hsvToRgb(float h, float s, float v) {
    float c = v * s;
    float x = c * (1.0 - abs(mod(h / 60.0, 2.0) - 1.0));
    float m = v - c;
    vec3 rgb;
    if      (h < 60.0)  rgb = vec3(c, x, 0.0);
    else if (h < 120.0) rgb = vec3(x, c, 0.0);
    else if (h < 180.0) rgb = vec3(0.0, c, x);
    else if (h < 240.0) rgb = vec3(0.0, x, c);
    else if (h < 300.0) rgb = vec3(x, 0.0, c);
    else                rgb = vec3(c, 0.0, x);
    return rgb + m;
}

void main() {
    float t = clamp((vZ - uMinZ) / max(uMaxZ - uMinZ, 1e-6), 0.0, 1.0);
    float hue = (1.0 - t) * 240.0;
    gl_FragColor = vec4(hsvToRgb(hue, 1.0, 1.0), 1.0);
}
)";

static const char* HOLE_VERT_SRC = R"(
#version 130
in vec3 aPos;
uniform mat4 uMVP;
void main() { gl_Position = uMVP * vec4(aPos, 1.0); }
)";

static const char* HOLE_FRAG_SRC = R"(
#version 130
uniform vec4 uColor;
void main() { gl_FragColor = uColor; }
)";

namespace {
static constexpr float GVIEWER_PI = 3.14159265358979323846f;

void genSphere(const cv::Point3f& c, float r, int stacks, int slices, std::vector<float>& out)
{
    for (int i = 0; i < stacks; ++i)
    {
        float phi0 = GVIEWER_PI * i / stacks;
        float phi1 = GVIEWER_PI * (i + 1) / stacks;
        for (int j = 0; j < slices; ++j)
        {
            float th0 = 2.0f * GVIEWER_PI * j / slices;
            float th1 = 2.0f * GVIEWER_PI * (j + 1) / slices;
            auto p = [&](float phi, float theta) {
                out.push_back(c.x + r * std::sin(phi) * std::cos(theta));
                out.push_back(c.y + r * std::sin(phi) * std::sin(theta));
                out.push_back(c.z + r * std::cos(phi));
            };
            p(phi0, th0); p(phi1, th0); p(phi1, th1);
            p(phi0, th0); p(phi1, th1); p(phi0, th1);
        }
    }
}

} // namespace

GlPCViewer::GlPCViewer(QWidget* parent)
    : QOpenGLWidget(parent)
    , m_vbo(QOpenGLBuffer::VertexBuffer)
{
    setMouseTracking(true);

    m_coordLabel = new QLabel(this);
    m_coordLabel->setStyleSheet("color: white; background: rgba(0,0,0,140); padding: 2px 6px;");
    m_coordLabel->setAlignment(Qt::AlignLeft);
    m_coordLabel->hide();
}

GlPCViewer::~GlPCViewer()
{
    makeCurrent();
    m_vbo.destroy();
    m_vao.destroy();
    m_sphereVbo.destroy();
    m_sphereVao.destroy();
    m_contourVbo.destroy();
    m_contourVao.destroy();
    m_planeVbo.destroy();
    m_planeVao.destroy();
    doneCurrent();
}

void GlPCViewer::initializeGL()
{
    initializeOpenGLFunctions();
    spdlog::info("[GL] Renderer: {} | Version: {} | GLSL: {}",
        reinterpret_cast<const char*>(glGetString(GL_RENDERER)),
        reinterpret_cast<const char*>(glGetString(GL_VERSION)),
        reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(0x8642);  // GL_PROGRAM_POINT_SIZE (OpenGL 3.2+)

    buildShader();

    m_vao.create();
    m_vbo.create();

    m_vao.bind();
    m_vbo.bind();
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    m_vao.release();
    m_vbo.release();

    // Box wireframe
    buildBoxShader();
    m_boxVao.create();
    m_boxVbo.create();
    m_boxVao.bind();
    m_boxVbo.bind();
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    m_boxVao.release();
    m_boxVbo.release();

    // Hole visualization
    buildHoleShader();

    m_sphereVao.create();
    m_sphereVbo.create();
    m_sphereVao.bind();
    m_sphereVbo.bind();
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    m_sphereVao.release();
    m_sphereVbo.release();

    m_contourVao.create();
    m_contourVbo.create();
    m_contourVao.bind();
    m_contourVbo.bind();
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    m_contourVao.release();
    m_contourVbo.release();

    m_planeVao.create();
    m_planeVbo.create();
    m_planeVao.bind();
    m_planeVbo.bind();
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    m_planeVao.release();
    m_planeVbo.release();
}

void GlPCViewer::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void GlPCViewer::resizeEvent(QResizeEvent* e)
{
    QOpenGLWidget::resizeEvent(e);
    if (m_coordLabel)
        m_coordLabel->move(width() - m_coordLabel->sizeHint().width() - 4,
                           height() - m_coordLabel->sizeHint().height() - 4);
}

void GlPCViewer::paintGL()
{
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (m_pointCount == 0) return;

    m_shader.bind();
    m_shader.setUniformValue("uMVP", buildMVP());
    m_shader.setUniformValue("uMinZ", m_minZ);
    m_shader.setUniformValue("uMaxZ", m_maxZ);

    m_vao.bind();
    glDrawArrays(GL_POINTS, 0, m_pointCount);
    m_vao.release();

    m_shader.release();

    // Crop box wireframe
    if (m_showCropBox)
        drawCropBox();

    // Reference plane drawing disabled (text label still shown below)

    // Hole / Bolt visualization
    if (m_showHole)
    {
        drawHole();
        drawContour();
    }
    if (m_showBolt)
    {
        drawHole();
        drawContour();
    }

    // 2D legend overlay
    QPainter p(this);
    drawLegend(p);

    // Helper: draw a label with dark background near a world position
    auto drawWorldLabel = [&](const cv::Point3f& worldPt, const QString& label) {
        QVector4D clip = buildMVP() * QVector4D(worldPt.x, worldPt.y, worldPt.z, 1.0f);
        if (clip.w() <= 0.0f) return;
        float sx = ( clip.x() / clip.w() + 1.0f) * 0.5f * width();
        float sy = (-clip.y() / clip.w() + 1.0f) * 0.5f * height();
        // clamp to widget bounds
        sx = std::max(4.0f, std::min(sx, static_cast<float>(width())  - 4.0f));
        sy = std::max(20.0f, std::min(sy, static_cast<float>(height()) - 4.0f));
        p.setFont(QFont("Consolas", 10, QFont::Bold));
        QRect textRect = p.fontMetrics().boundingRect(label);
        int tx = static_cast<int>(sx) - textRect.width() / 2;
        int ty = static_cast<int>(sy) - 8;
        tx = std::max(4, std::min(tx, width() - textRect.width() - 4));
        QRect bg(tx - 4, ty - textRect.height(), textRect.width() + 8, textRect.height() + 4);
        p.fillRect(bg, QColor(0, 0, 0, 180));
        p.setPen(QColor(255, 220, 50));
        p.drawText(tx, ty, label);
    };

    // "Plane" label: project planeCenter to screen, offset 20px below center
    if (m_showPlane)
    {
        const cv::Point3f& pc = m_showHole ? m_holeResult.planeCenter : m_boltResult.planeCenter;
        QVector4D clip = buildMVP() * QVector4D(pc.x, pc.y, pc.z, 1.0f);
        if (clip.w() > 0.0f)
        {
            float sx = ( clip.x() / clip.w() + 1.0f) * 0.5f * width();
            float sy = (-clip.y() / clip.w() + 1.0f) * 0.5f * height() + 20.0f;  // 20px below center
            sx = std::max(4.0f, std::min(sx, static_cast<float>(width())  - 4.0f));
            sy = std::max(20.0f, std::min(sy, static_cast<float>(height()) - 4.0f));
            const QString planeLabel = "Plane";
            p.setFont(QFont("Consolas", 10, QFont::Bold));
            QRect tr = p.fontMetrics().boundingRect(planeLabel);
            int tx = static_cast<int>(sx) - tr.width() / 2;
            int ty = static_cast<int>(sy);
            tx = std::max(4, std::min(tx, width() - tr.width() - 4));
            QRect bg(tx - 4, ty - tr.height(), tr.width() + 8, tr.height() + 4);
            p.fillRect(bg, QColor(0, 0, 0, 180));
            p.setPen(QColor(100, 200, 255));
            p.drawText(tx, ty, planeLabel);
        }
    }

    if (m_showHole)
    {
        const cv::Point3f& c = m_holeResult.center;
        drawWorldLabel(c, QString("Hole Center: (%1, %2, %3)")
            .arg(static_cast<double>(c.x), 0, 'f', 2)
            .arg(static_cast<double>(c.y), 0, 'f', 2)
            .arg(static_cast<double>(c.z), 0, 'f', 2));
    }

    if (m_showBolt)
    {
        const cv::Point3f& c = m_boltResult.center;
        drawWorldLabel(c, QString("Bolt Height: %1 mm")
            .arg(static_cast<double>(m_boltResult.height), 0, 'f', 3));
    }

    // Draw control hints last so they appear on top of all other overlays
    drawControlHints(p);
}

void GlPCViewer::setPointCloud(const PCData& pc)
{
    m_minZ = pc.minZ;
    m_maxZ = pc.maxZ;

    std::vector<float> pts;
    pts.reserve(pc.pointCount * 3);

    float minX = std::numeric_limits<float>::max(),  maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max(),  maxY = std::numeric_limits<float>::lowest();

    // Upload all valid points to VBO without downsampling
    for (int i = 0; i < pc.matPointCloud.rows; ++i)
        for (int j = 0; j < pc.matPointCloud.cols; ++j) {
            const auto& v = pc.matPointCloud.at<cv::Vec3f>(i, j);
            if (isValidPoint(v)) {
                minX = std::min(minX, v[0]); maxX = std::max(maxX, v[0]);
                minY = std::min(minY, v[1]); maxY = std::max(maxY, v[1]);
                pts.push_back(v[0]);
                pts.push_back(v[1]);
                pts.push_back(v[2]);
            }
        }

    m_pointCount = static_cast<int>(pts.size() / 3);
    if (m_pointCount == 0) return;

    m_showHole = false;
    m_showBolt = false;
    m_showCropBox = false;
    m_showPlane = false;

    m_center = { (minX + maxX) * 0.5f, (minY + maxY) * 0.5f, (pc.minZ + pc.maxZ) * 0.5f };
    m_target  = m_center;
    const float diagXY = std::hypot(maxX - minX, maxY - minY);
    const float diagZ  = pc.maxZ - pc.minZ;
    m_zoom = std::max(diagXY, std::max(diagZ, 1.0f)) * 1.5f;

    makeCurrent();
    m_vbo.bind();
    m_vbo.allocate(pts.data(), static_cast<int>(pts.size() * sizeof(float)));
    m_vbo.release();
    doneCurrent();

    update();
}

void GlPCViewer::buildShader()
{
    if (!m_shader.addShaderFromSourceCode(QOpenGLShader::Vertex,   VERT_SRC))
        spdlog::error("[GL] Point vertex shader compile failed: {}", m_shader.log().toStdString());
    if (!m_shader.addShaderFromSourceCode(QOpenGLShader::Fragment, FRAG_SRC))
        spdlog::error("[GL] Point fragment shader compile failed: {}", m_shader.log().toStdString());
    // Explicitly bind attribute location before linking (required for GLSL 130 without layout qualifier)
    m_shader.bindAttributeLocation("aPos", 0);
    if (!m_shader.link())
        spdlog::error("[GL] Point shader link failed: {}", m_shader.log().toStdString());
    else
        spdlog::info("[GL] Point shader compiled OK");
}

void GlPCViewer::buildBoxShader()
{
    m_boxShader.addShaderFromSourceCode(QOpenGLShader::Vertex,   BOX_VERT_SRC);
    m_boxShader.addShaderFromSourceCode(QOpenGLShader::Fragment, BOX_FRAG_SRC);
    m_boxShader.bindAttributeLocation("aPos", 0);
    if (!m_boxShader.link())
        spdlog::error("[General] GlPCViewer box shader link failed: {}", m_boxShader.log().toStdString());
}

void GlPCViewer::updateBoxVbo()
{
    const float cx = m_cropCenter.x(), cy = m_cropCenter.y(), cz = m_cropCenter.z();
    const float hx = m_cropSize.x() * 0.5f;
    const float hy = m_cropSize.y() * 0.5f;
    const float hz = m_cropSize.z() * 0.5f;

    // 8 corners
    float v[8][3] = {
        {cx-hx, cy-hy, cz-hz}, {cx+hx, cy-hy, cz-hz},
        {cx+hx, cy+hy, cz-hz}, {cx-hx, cy+hy, cz-hz},
        {cx-hx, cy-hy, cz+hz}, {cx+hx, cy-hy, cz+hz},
        {cx+hx, cy+hy, cz+hz}, {cx-hx, cy+hy, cz+hz},
    };
    // 12 edges x 2 vertices = 24 x 3 floats
    int idx[12][2] = {
        {0,1},{1,2},{2,3},{3,0},  // bottom
        {4,5},{5,6},{6,7},{7,4},  // top
        {0,4},{1,5},{2,6},{3,7},  // verticals
    };
    float pts[24 * 3];
    int p = 0;
    for (auto& e : idx) {
        pts[p++]=v[e[0]][0]; pts[p++]=v[e[0]][1]; pts[p++]=v[e[0]][2];
        pts[p++]=v[e[1]][0]; pts[p++]=v[e[1]][1]; pts[p++]=v[e[1]][2];
    }

    makeCurrent();
    m_boxVbo.bind();
    m_boxVbo.allocate(pts, sizeof(pts));
    m_boxVbo.release();
    doneCurrent();
}

void GlPCViewer::drawCropBox()
{
    glLineWidth(2.5f);
    m_boxShader.bind();
    m_boxShader.setUniformValue("uMVP", buildMVP());
    m_boxVao.bind();
    glDrawArrays(GL_LINES, 0, 24);
    m_boxVao.release();
    m_boxShader.release();
    glLineWidth(1.0f);
}

void GlPCViewer::setCropBox(float cx, float cy, float cz, float sx, float sy, float sz)
{
    m_cropCenter = {cx, cy, cz};
    m_cropSize   = {sx, sy, sz};
    m_showCropBox = true;
    updateBoxVbo();
    update();
}

void GlPCViewer::hideCropBox()
{
    m_showCropBox = false;
    update();
}

void GlPCViewer::focusOnBox(float cx, float cy, float cz, float sx, float sy, float sz)
{
    m_target = QVector3D(cx, cy, cz);
    m_zoom   = std::max({sx, sy, sz, 1.0f}) * 2.0f;
    update();
}

QVector3D GlPCViewer::pickWorld(int x, int y, bool& valid)
{
    valid = false;

    constexpr int SEARCH_RADIUS = 20;
    const int w = width(), h = height();
    if (w <= 0 || h <= 0) return {};

    const int rx0 = std::max(0, x - SEARCH_RADIUS);
    const int ry0 = std::max(0, y - SEARCH_RADIUS);
    const int rx1 = std::min(w - 1, x + SEARCH_RADIUS);
    const int ry1 = std::min(h - 1, y + SEARCH_RADIUS);
    const int bw  = rx1 - rx0 + 1;
    const int bh  = ry1 - ry0 + 1;
    if (bw <= 0 || bh <= 0) return {};

    std::vector<float> depthBuf(static_cast<size_t>(bw * bh), 1.0f);
    // Flip Y: OpenGL Y is bottom-up
    glReadPixels(rx0, h - ry1 - 1, bw, bh, GL_DEPTH_COMPONENT, GL_FLOAT, depthBuf.data());

    // Find the closest rendered pixel to the cursor within search radius
    float bestDepth = 1.0f;
    int bestPx = x, bestPy = y;
    int bestDist2 = INT_MAX;

    for (int dy = 0; dy < bh; ++dy)
    {
        for (int dx = 0; dx < bw; ++dx)
        {
            float d = depthBuf[static_cast<size_t>(dy * bw + dx)];
            if (d >= 1.0f) continue;

            // OpenGL depth buffer rows are bottom-up; convert to screen Y
            int screenY = ry1 - dy;
            int screenX = rx0 + dx;
            int dist2 = (screenX - x) * (screenX - x) + (screenY - y) * (screenY - y);
            if (dist2 < bestDist2) {
                bestDist2 = dist2;
                bestDepth = d;
                bestPx = screenX;
                bestPy = screenY;
            }
        }
    }

    if (bestDepth >= 1.0f) return {};

    const float ndcX =  2.0f * bestPx / w  - 1.0f;
    const float ndcY =  1.0f - 2.0f * bestPy / h;
    const float ndcZ =  2.0f * bestDepth - 1.0f;

    bool ok;
    QVector4D wv = buildMVP().inverted(&ok) * QVector4D(ndcX, ndcY, ndcZ, 1.0f);
    if (!ok) return {};
    wv /= wv.w();
    valid = true;
    return {wv.x(), wv.y(), wv.z()};
}

void GlPCViewer::drawLegend(QPainter& p)
{
    if (m_pointCount == 0) return;

    const int barW   = 12;
    const int barH   = 220;
    const int margin = 16;
    const int labelH = 16;
    const int x = width() - barW - margin;
    const int y = (height() - barH) / 2;

    p.setRenderHint(QPainter::Antialiasing);

    // Gradient bar (red top -> purple bottom)
    QLinearGradient grad(0, y, 0, y + barH);
    grad.setColorAt(0.0,        QColor(255,   0,   0));
    grad.setColorAt(1.0 / 6.0, QColor(255, 165,   0));
    grad.setColorAt(2.0 / 6.0, QColor(255, 255,   0));
    grad.setColorAt(3.0 / 6.0, QColor(  0, 255,   0));
    grad.setColorAt(4.0 / 6.0, QColor(  0,   0, 255));
    grad.setColorAt(1.0,        QColor(128,   0, 128));

    QPainterPath path;
    path.addRoundedRect(x, y, barW, barH, 3, 3);
    p.fillPath(path, grad);

    // Label helper: dark bg + white text
    auto drawLabel = [&](const QRect& rect, const QString& text) {
        QRect bg = rect.adjusted(-3, -1, 3, 1);
        p.fillRect(bg, QColor(0, 0, 0, 160));
        p.setPen(Qt::white);
        p.setFont(QFont("Consolas", 8, QFont::Bold));
        p.drawText(rect, Qt::AlignCenter, text);
    };

    QRect topRect(x - 34, y - labelH - 1, barW + 34, labelH);
    drawLabel(topRect, QString::number(m_maxZ, 'f', 2));

    QRect botRect(x - 34, y + barH + 1, barW + 34, labelH);
    drawLabel(botRect, QString::number(m_minZ, 'f', 2));
}

void GlPCViewer::drawControlHints(QPainter& p)
{
    if (m_pointCount == 0) return;

    static const char* kHints[] = {
        "Mid drag   : Rotate",
        "Right drag : Pan",
        "Scroll     : Zoom",
        "Ctrl+Click : Set crop center",
    };
    p.setFont(QFont("Consolas", 10));
    const int hintX = 8;
    int hintY = height() - 8;
    for (int i = 3; i >= 0; --i)
    {
        int textW = p.fontMetrics().horizontalAdvance(kHints[i]);
        int textH = p.fontMetrics().height();
        hintY -= textH + 3;
        QRect bg(hintX - 3, hintY - 1, textW + 10, textH + 3);
        p.fillRect(bg, QColor(0, 0, 0, 200));
        p.setPen(QColor(255, 255, 255));
        p.drawText(hintX + 2, hintY + p.fontMetrics().ascent(), kHints[i]);
    }
}

QMatrix4x4 GlPCViewer::buildMVP() const
{
    const float aspect = width() > 0 ? static_cast<float>(width()) / height() : 1.0f;
    QMatrix4x4 proj, view;
    proj.perspective(45.0f, aspect, m_zoom * 0.01f, m_zoom * 1000.0f);
    QVector3D eye = m_rotation.rotatedVector(QVector3D(0.0f, 0.0f, m_zoom));
    QVector3D up  = m_rotation.rotatedVector(QVector3D(0.0f, 1.0f, 0.0f));
    view.lookAt(m_target + eye, m_target, up);
    return proj * view;
}

void GlPCViewer::mousePressEvent(QMouseEvent* e)
{
    m_lastMouse = e->pos();

    // Ctrl+click: pick world position and set as crop center
    if (e->button() == Qt::LeftButton && (e->modifiers() & Qt::ControlModifier))
    {
        makeCurrent();
        bool valid;
        QVector3D w = pickWorld(e->pos().x(), e->pos().y(), valid);
        doneCurrent();
        if (valid)
            emit cropCenterPicked(w.x(), w.y(), w.z());
    }
}

void GlPCViewer::mouseMoveEvent(QMouseEvent* e)
{
    const QPoint delta = e->pos() - m_lastMouse;

    if (e->buttons() & Qt::MiddleButton) {
        // Trackball rotation: axes derived from current camera orientation
        const float angleH = -static_cast<float>(delta.x()) * 0.3f;
        const float angleV = -static_cast<float>(delta.y()) * 0.3f;
        QVector3D upV    = m_rotation.rotatedVector(QVector3D(0.0f, 1.0f, 0.0f));
        QVector3D rightV = m_rotation.rotatedVector(QVector3D(1.0f, 0.0f, 0.0f));
        QQuaternion rotH = QQuaternion::fromAxisAndAngle(upV,    angleH);
        QQuaternion rotV = QQuaternion::fromAxisAndAngle(rightV, angleV);
        m_rotation = (rotH * rotV * m_rotation).normalized();
        update();
    }
    else if (e->buttons() & Qt::RightButton) {
        // Pan: move m_target along camera right/up axes
        const float panScale = m_zoom / height();
        QVector3D right = m_rotation.rotatedVector(QVector3D(1.0f, 0.0f, 0.0f));
        QVector3D up    = m_rotation.rotatedVector(QVector3D(0.0f, 1.0f, 0.0f));
        m_target -= right * (static_cast<float>(delta.x()) * panScale);
        m_target += up    * (static_cast<float>(delta.y()) * panScale);
        update();
    }

    m_lastMouse = e->pos();
    showCoordinates(e->pos().x(), e->pos().y());
}

void GlPCViewer::showCoordinates(int x, int y)
{
    if (m_pointCount == 0) return;

    makeCurrent();
    bool valid;
    QVector3D w = pickWorld(x, y, valid);
    doneCurrent();

    if (!valid) {
        m_coordLabel->hide();
        return;
    }

    m_coordLabel->setText(QString("X: %1  Y: %2  Z: %3")
        .arg(w.x(), 0, 'f', 3)
        .arg(w.y(), 0, 'f', 3)
        .arg(w.z(), 0, 'f', 3));
    m_coordLabel->adjustSize();
    m_coordLabel->move(width() - m_coordLabel->width() - 4, height() - m_coordLabel->height() - 4);
    m_coordLabel->show();
    m_coordLabel->raise();
}

void GlPCViewer::wheelEvent(QWheelEvent* e)
{
    m_zoom *= (e->angleDelta().y() > 0) ? 0.9f : 1.1f;
    m_zoom  = std::max(0.001f, m_zoom);
    update();
}

void GlPCViewer::buildHoleShader()
{
    m_holeShader.addShaderFromSourceCode(QOpenGLShader::Vertex,   HOLE_VERT_SRC);
    m_holeShader.addShaderFromSourceCode(QOpenGLShader::Fragment, HOLE_FRAG_SRC);
    m_holeShader.bindAttributeLocation("aPos", 0);
    if (!m_holeShader.link())
        spdlog::error("[General] GlPCViewer hole shader link failed: {}", m_holeShader.log().toStdString());
}

static void buildPlaneQuad(const cv::Point3f& center, const cv::Vec3f& normal,
                            float hx, float hy,
                            std::vector<float>& out, QVector3D& labelCorner)
{
    QVector3D n(normal[0], normal[1], normal[2]);
    n.normalize();
    QVector3D up(0.0f, 0.0f, 1.0f);
    if (std::abs(QVector3D::dotProduct(n, up)) > 0.9f)
        up = QVector3D(1.0f, 0.0f, 0.0f);
    QVector3D u = QVector3D::crossProduct(n, up).normalized();
    QVector3D v = QVector3D::crossProduct(n, u).normalized();

    QVector3D c(center.x, center.y, center.z);
    QVector3D p0 = c - u * hx - v * hy;
    QVector3D p1 = c + u * hx - v * hy;
    QVector3D p2 = c + u * hx + v * hy;
    QVector3D p3 = c - u * hx + v * hy;
    labelCorner = p1;  // opposite corner from p3

    auto push = [&](const QVector3D& p) {
        out.push_back(p.x()); out.push_back(p.y()); out.push_back(p.z());
    };
    push(p0); push(p1); push(p2);
    push(p0); push(p2); push(p3);
}

void GlPCViewer::setHoleResult(const Circle3D& circle)
{
    m_holeResult = circle;
    m_showHole   = true;
    m_showBolt   = false;

    float sphereR = std::max(0.1f, circle.radius * 0.05f);
    std::vector<float> sphereVerts;
    genSphere(circle.center, sphereR, 12, 24, sphereVerts);

    m_sphereVertCount = static_cast<int>(sphereVerts.size() / 3);

    // Build contour line loop from 3D contour points
    std::vector<float> contourVerts;
    contourVerts.reserve(circle.contourPoints.size() * 3);
    for (const auto& p : circle.contourPoints)
    {
        contourVerts.push_back(p.x);
        contourVerts.push_back(p.y);
        contourVerts.push_back(p.z);
    }
    m_contourVertCount = static_cast<int>(contourVerts.size() / 3);

    // Plane quad: use smallest crop dimension * 0.5 so tilted planes stay inside box
    std::vector<float> planeVerts;
    float halfPlane;
    if (m_cropSize.x() > 0.0f)
        halfPlane = std::min({m_cropSize.x(), m_cropSize.y(), m_cropSize.z()}) * 0.25f;
    else
        halfPlane = std::max(circle.radius * 4.0f, 20.0f);
    cv::Vec3f pn = (circle.planeNormal[0] != 0.0f || circle.planeNormal[1] != 0.0f || circle.planeNormal[2] != 0.0f)
        ? circle.planeNormal : circle.normal;
    buildPlaneQuad(circle.planeCenter, pn, halfPlane, halfPlane, planeVerts, m_planeLabelPos);
    m_planeVertCount = static_cast<int>(planeVerts.size() / 3);
    m_showPlane = true;

    makeCurrent();
    m_sphereVbo.bind();
    m_sphereVbo.allocate(sphereVerts.data(), static_cast<int>(sphereVerts.size() * sizeof(float)));
    m_sphereVbo.release();
    if (!contourVerts.empty())
    {
        m_contourVbo.bind();
        m_contourVbo.allocate(contourVerts.data(), static_cast<int>(contourVerts.size() * sizeof(float)));
        m_contourVbo.release();
    }
    if (!planeVerts.empty())
    {
        m_planeVbo.bind();
        m_planeVbo.allocate(planeVerts.data(), static_cast<int>(planeVerts.size() * sizeof(float)));
        m_planeVbo.release();
    }
    doneCurrent();

    update();
}

void GlPCViewer::setBoltResult(const Circle3D& circle)
{
    m_boltResult = circle;
    m_showBolt   = true;
    m_showHole   = false;

    float sphereR = std::max(0.1f, circle.radius * 0.05f);
    std::vector<float> sphereVerts;
    genSphere(circle.center, sphereR, 12, 24, sphereVerts);
    m_sphereVertCount = static_cast<int>(sphereVerts.size() / 3);

    std::vector<float> contourVerts;
    contourVerts.reserve(circle.contourPoints.size() * 3);
    for (const auto& p : circle.contourPoints)
    {
        contourVerts.push_back(p.x);
        contourVerts.push_back(p.y);
        contourVerts.push_back(p.z);
    }
    m_contourVertCount = static_cast<int>(contourVerts.size() / 3);

    // Plane quad: use smallest crop dimension * 0.5 so tilted planes stay inside box
    std::vector<float> planeVerts;
    float halfPlane2;
    if (m_cropSize.x() > 0.0f)
        halfPlane2 = std::min({m_cropSize.x(), m_cropSize.y(), m_cropSize.z()}) * 0.25f;
    else
        halfPlane2 = std::max(circle.radius * 4.0f, 20.0f);
    cv::Vec3f pn2 = (circle.planeNormal[0] != 0.0f || circle.planeNormal[1] != 0.0f || circle.planeNormal[2] != 0.0f)
        ? circle.planeNormal : circle.normal;
    buildPlaneQuad(circle.planeCenter, pn2, halfPlane2, halfPlane2, planeVerts, m_planeLabelPos);
    m_planeVertCount = static_cast<int>(planeVerts.size() / 3);
    m_showPlane = true;

    makeCurrent();
    m_sphereVbo.bind();
    m_sphereVbo.allocate(sphereVerts.data(), static_cast<int>(sphereVerts.size() * sizeof(float)));
    m_sphereVbo.release();
    if (!contourVerts.empty())
    {
        m_contourVbo.bind();
        m_contourVbo.allocate(contourVerts.data(), static_cast<int>(contourVerts.size() * sizeof(float)));
        m_contourVbo.release();
    }
    if (!planeVerts.empty())
    {
        m_planeVbo.bind();
        m_planeVbo.allocate(planeVerts.data(), static_cast<int>(planeVerts.size() * sizeof(float)));
        m_planeVbo.release();
    }
    doneCurrent();

    update();
}

void GlPCViewer::clearResults()
{
    m_showHole         = false;
    m_showBolt         = false;
    m_showCropBox      = false;
    m_showPlane        = false;
    m_sphereVertCount  = 0;
    m_contourVertCount = 0;
    m_planeVertCount   = 0;
    update();
}


void GlPCViewer::drawPlane()
{
    if (m_planeVertCount < 3) return;

    glDisable(GL_DEPTH_TEST);   // show plane through the point cloud
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_holeShader.bind();
    m_holeShader.setUniformValue("uMVP", buildMVP());
    m_holeShader.setUniformValue("uColor", QVector4D(0.4f, 0.7f, 1.0f, 0.5f));
    m_planeVao.bind();
    glDrawArrays(GL_TRIANGLES, 0, m_planeVertCount);
    m_planeVao.release();
    m_holeShader.release();

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void GlPCViewer::drawHole()
{
    m_holeShader.bind();
    m_holeShader.setUniformValue("uMVP", buildMVP());

    // Sphere: solid red at hole center
    m_holeShader.setUniformValue("uColor", QVector4D(1.0f, 0.2f, 0.2f, 1.0f));
    m_sphereVao.bind();
    glDrawArrays(GL_TRIANGLES, 0, m_sphereVertCount);
    m_sphereVao.release();

    m_holeShader.release();
}

void GlPCViewer::drawContour()
{
    if (m_contourVertCount < 2) return;

    m_holeShader.bind();
    m_holeShader.setUniformValue("uMVP", buildMVP());
    m_holeShader.setUniformValue("uColor", QVector4D(1.0f, 1.0f, 0.0f, 1.0f));  // yellow

    glLineWidth(4.0f);
    m_contourVao.bind();
    glDrawArrays(GL_LINE_LOOP, 0, m_contourVertCount);
    m_contourVao.release();
    glLineWidth(1.0f);

    m_holeShader.release();
}
