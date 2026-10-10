#pragma once

#include <QMainWindow>
#include "ImageViewer.h"
#include "ui_MainWindow.h"

#include "MainController.h"
#include "QtSpdlogSink.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindowClass; };
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget *parent = nullptr);
	~MainWindow();

signals:
	void openFileRequested(const QString& filePath);
	void cropRequested(float cx, float cy, float cz, float sx, float sy, float sz);
	void holeSearchRequested(double distThreshold);
	void boltSearchRequested(BoltSearchParams params);

private slots:
	void onOpenButtonClicked();
	void onRunPresetClicked();
	void onRegisterPresetClicked();
	void onAddPresetClicked();

private:
	struct PointEntry {
		QString filePath;   // point cloud file for this point
		QString type;       // "Hole" or "Bolt"
		float   cx, cy, cz;
		float   sx, sy, sz;
		float   floorRemovalDist;
		float   tolerance;  // Hole: plane fit tolerance, Bolt: surface layer
	};

	struct PresetEntry {
		QString           name;
		QList<PointEntry> points;
	};

	void loadSettings();
	void saveSettings();
	void loadPresets();
	void saveAllPresets();
	void runDemoSearch(const QString& presetName, const PointEntry& pt);
	void updateDemoProgressLabel();
	void updateResultPanel(const QString& type, const Circle3D& c);
	void clearResultPanel();
	void closeEvent(QCloseEvent* event) override;
	void dragEnterEvent(QDragEnterEvent* event) override;
	void dragMoveEvent(QDragMoveEvent* event) override;
	void dropEvent(QDropEvent* event) override;

	// Returns the first droppable point cloud file path, empty if the payload has none
	static QString droppedPointCloudPath(const QMimeData* mime);

	Ui::MainWindowClass *ui;
	MainController* m_controller;
	QList<PresetEntry> m_presets;
	int m_demoPhase         = 0;
	int m_demoPointIdx      = 0;
	int m_demoSearchedCount = 0;
	ImageViewer* m_imageViewer = nullptr;

	// Last search result for +Register
	bool          m_hasSearchResult  = false;
	QString       m_lastSearchType;   // "Hole" or "Bolt"
	Circle3D      m_lastSearchResult;
};
