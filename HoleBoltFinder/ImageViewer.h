#pragma once

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QTimer>

class ImageViewer : public QGraphicsView
{
public:
	explicit ImageViewer(QWidget* parent = nullptr);

	void setImage(const QPixmap& pixmap);
	void resetZoom();

protected:
	void wheelEvent(QWheelEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;
	void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
	void applyZoom();

	QGraphicsScene*      m_scene      = nullptr;
	QGraphicsPixmapItem* m_pixmapItem = nullptr;
	bool                 m_hasImage   = false;
	double               m_zoomFactor = 1.0;
};
