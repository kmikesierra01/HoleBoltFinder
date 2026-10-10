#include "pch.h"
#include "ImageViewer.h"
#include <algorithm>

ImageViewer::ImageViewer(QWidget* parent)
	: QGraphicsView(parent)
{
	m_scene = new QGraphicsScene(this);
	setScene(m_scene);

	setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
	setResizeAnchor(QGraphicsView::AnchorViewCenter);
	setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
	setDragMode(QGraphicsView::ScrollHandDrag);
	setAcceptDrops(false);              // let file drops fall through to MainWindow
	viewport()->setAcceptDrops(false);
	setFrameShape(QFrame::NoFrame);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	setStyleSheet("background-color:#1a2332; border:1px solid #334155; border-radius:6px;");

	m_pixmapItem = m_scene->addPixmap(QPixmap());
	m_pixmapItem->setTransformationMode(Qt::SmoothTransformation);
}

void ImageViewer::setImage(const QPixmap& pixmap)
{
	m_pixmapItem->setPixmap(pixmap);
	m_scene->setSceneRect(m_pixmapItem->boundingRect());
	m_hasImage = !pixmap.isNull();
	m_zoomFactor = 1.0;
	applyZoom();
}

void ImageViewer::resetZoom()
{
	m_zoomFactor = 1.0;
	applyZoom();
}

// applyZoom: fit to view first, then scale by m_zoomFactor on top.
// This means zoom is always relative to "fit" size, so resize recalculates correctly.
void ImageViewer::applyZoom()
{
	if (!m_hasImage || m_scene->sceneRect().isEmpty()) return;
	resetTransform();
	fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
	if (m_zoomFactor != 1.0)
		scale(m_zoomFactor, m_zoomFactor);
}

void ImageViewer::wheelEvent(QWheelEvent* event)
{
	const double step      = (event->angleDelta().y() > 0) ? 1.07 : (1.0 / 1.07);
	const double newFactor = std::clamp(m_zoomFactor * step, 1.0, 5.0);
	if (newFactor == m_zoomFactor) return;  // no-op: zoom factor unchanged
	m_zoomFactor = newFactor;
	applyZoom();
}

void ImageViewer::resizeEvent(QResizeEvent* event)
{
	QGraphicsView::resizeEvent(event);
	// Defer until layout is settled to get the correct viewport size.
	QTimer::singleShot(0, this, [this] { applyZoom(); });
}

void ImageViewer::mouseDoubleClickEvent(QMouseEvent* event)
{
	QGraphicsView::mouseDoubleClickEvent(event);
	resetZoom();
}
