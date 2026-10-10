#include "pch.h"
#include "MainWindow.h"
#include "GlPCViewer.h"
#include "ImageViewer.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QSettings>
#include <QCoreApplication>
#include <QApplication>
#include <QTimer>
#include <QTime>
#include <QDate>
#include <QSplitter>
#include <QResizeEvent>
#include <QCloseEvent>
#include <QInputDialog>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindowClass())
{
	ui->setupUi(this);

	// Load point cloud files dropped from Explorer
	setAcceptDrops(true);

	// 3D viewer (left) + 2D image viewer (right), splitter ratio 3:1
	auto* viewerSplitter = new QSplitter(Qt::Vertical, this);
	viewerSplitter->setHandleWidth(4);
	viewerSplitter->setChildrenCollapsible(true);
	viewerSplitter->addWidget(ui->openGLWidget);
	ui->openGLWidget->setMinimumSize(0, 0);  // override minimum size set by UI designer

	m_imageViewer = new ImageViewer(viewerSplitter);
	m_imageViewer->setMinimumSize(0, 0);
	viewerSplitter->addWidget(m_imageViewer);
	viewerSplitter->setStretchFactor(0, 3);
	viewerSplitter->setStretchFactor(1, 1);
	viewerSplitter->setSizes({300, 100});

	// Viewer label overlays
	const QString kLabelStyle =
		"background:rgba(0,0,0,75); color:rgba(210,210,210,200);"
		"padding:2px 7px; border-radius:3px; font-size:9pt;";

	auto* lbl3D = new QLabel("3D View", ui->openGLWidget);
	lbl3D->setStyleSheet(kLabelStyle);
	lbl3D->setAttribute(Qt::WA_TransparentForMouseEvents);
	lbl3D->adjustSize();
	lbl3D->move(8, 8);
	lbl3D->raise();

	auto* lbl2D = new QLabel("2D View", m_imageViewer->viewport());
	lbl2D->setStyleSheet(kLabelStyle);
	lbl2D->setAttribute(Qt::WA_TransparentForMouseEvents);
	lbl2D->adjustSize();
	lbl2D->move(8, 8);
	lbl2D->raise();

	ui->splitter->insertWidget(0, viewerSplitter);
	ui->splitter->setStretchFactor(0, 55);
	ui->splitter->setStretchFactor(1, 45);
	ui->splitter->setSizes({550, 450});

	m_controller = new MainController(this);

	// Button Click
	connect(ui->pbFileOpen, &QPushButton::clicked, this, &MainWindow::onOpenButtonClicked);
	connect(ui->pbSaveSetting, &QPushButton::clicked, this, &MainWindow::saveSettings);
	// Viewer <- Controller
	connect(m_controller, &MainController::pointCloudLoaded, ui->openGLWidget, &GlPCViewer::setPointCloud);

	// CropPos spinbox -> GL box update
	auto updateCropBox = [this]() {
		ui->openGLWidget->setCropBox(
			static_cast<float>(ui->dsbCropPosX->value()),
			static_cast<float>(ui->dsbCropPosY->value()),
			static_cast<float>(ui->dsbCropPosZ->value()),
			static_cast<float>(ui->sbCropX->value()),
			static_cast<float>(ui->sbCropY->value()),
			static_cast<float>(ui->sbCropZ->value())
		);
	};
	connect(ui->dsbCropPosX, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, updateCropBox);
	connect(ui->dsbCropPosY, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, updateCropBox);
	connect(ui->dsbCropPosZ, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, updateCropBox);
	connect(ui->sbCropX,     QOverload<int>::of(&QSpinBox::valueChanged),          this, updateCropBox);
	connect(ui->sbCropY,     QOverload<int>::of(&QSpinBox::valueChanged),          this, updateCropBox);
	connect(ui->sbCropZ,     QOverload<int>::of(&QSpinBox::valueChanged),          this, updateCropBox);

	// Ctrl+click picking -> set CropPos spinbox
	connect(ui->openGLWidget, &GlPCViewer::cropCenterPicked, this, [this](float x, float y, float z) {
		ui->dsbCropPosX->setValue(static_cast<double>(x));
		ui->dsbCropPosY->setValue(static_cast<double>(y));
		ui->dsbCropPosZ->setValue(static_cast<double>(z));
	});

	// Emission
	connect(this, &MainWindow::openFileRequested, m_controller, &MainController::onOpenFileRequested);
	connect(this, &MainWindow::cropRequested, m_controller, &MainController::onCropRequested);
	connect(this, &MainWindow::holeSearchRequested, m_controller, &MainController::onHoleSearchRequested);
	connect(this, &MainWindow::boltSearchRequested, m_controller, &MainController::onBoltSearchRequested);

	// Hole search button: silent crop (no viewer update) then search
	connect(ui->pbSearchHole, &QPushButton::clicked, this, [this]() {
		const float cx = static_cast<float>(ui->dsbCropPosX->value());
		const float cy = static_cast<float>(ui->dsbCropPosY->value());
		const float cz = static_cast<float>(ui->dsbCropPosZ->value());
		const float sx = static_cast<float>(ui->sbCropX->value());
		const float sy = static_cast<float>(ui->sbCropY->value());
		const float sz = static_cast<float>(ui->sbCropZ->value());
		emit cropRequested(cx, cy, cz, sx, sy, sz);
		ui->openGLWidget->focusOnBox(cx, cy, cz, sx, sy, sz);
		ui->lbStatus->setText("Searching...");
		emit holeSearchRequested(ui->dsbHoleRansacDist->value());
	});

	// Bolt search button: silent crop (no viewer update) then search
	connect(ui->pbSearchBolt, &QPushButton::clicked, this, [this]() {
		const float cx = static_cast<float>(ui->dsbCropPosX->value());
		const float cy = static_cast<float>(ui->dsbCropPosY->value());
		const float cz = static_cast<float>(ui->dsbCropPosZ->value());
		const float sx = static_cast<float>(ui->sbCropX->value());
		const float sy = static_cast<float>(ui->sbCropY->value());
		const float sz = static_cast<float>(ui->sbCropZ->value());
		emit cropRequested(cx, cy, cz, sx, sy, sz);
		ui->openGLWidget->focusOnBox(cx, cy, cz, sx, sy, sz);
		BoltSearchParams p;
		p.floorRemovalDist = static_cast<float>(ui->dsbBoltFloorRemoval->value());
		p.surfaceLayer     = static_cast<float>(ui->dsbBoltSurfaceLayer->value());
		ui->lbStatus->setText("Searching...");
		emit boltSearchRequested(p);
	});

	// Hole result -> viewer
	connect(m_controller, &MainController::holeFound, ui->openGLWidget, &GlPCViewer::setHoleResult);

	// Bolt result -> viewer
	connect(m_controller, &MainController::boltFound, ui->openGLWidget, &GlPCViewer::setBoltResult);

	// Search/crop fail -> clear previous results from viewer
	connect(m_controller, &MainController::searchFailed, ui->openGLWidget, &GlPCViewer::clearResults);

	// Status bar
	auto statusTime = []() { return QTime::currentTime().toString("HH:mm:ss"); };
	connect(m_controller, &MainController::pointCloudLoaded, this, [this, statusTime](const PCData& pc) {
		ui->lbStatus->setText(statusTime() + "  |  PointCloud Loaded");
		ui->lbPointCount->setText(QLocale().toString(pc.pointCount));
	});
	connect(m_controller, &MainController::cropDone, this, [this, statusTime](int count) {
		ui->lbStatus->setText(statusTime() + "  |  Crop Done");
		ui->lbPointCount->setText(QLocale().toString(count));
	});
	connect(m_controller, &MainController::holeFound, this, [this, statusTime](const Circle3D& c) {
		ui->lbStatus->setText(statusTime() + "  |  Hole Found");
		m_lastSearchResult = c;
		m_lastSearchType   = "Hole";
		m_hasSearchResult  = true;
		ui->pbRegisterPreset->setEnabled(true);
		updateResultPanel("Hole", c);
	});
	connect(m_controller, &MainController::boltFound, this, [this, statusTime](const Circle3D& c) {
		ui->lbStatus->setText(statusTime() + "  |  Bolt Found");
		m_lastSearchResult = c;
		m_lastSearchType   = "Bolt";
		m_hasSearchResult  = true;
		ui->pbRegisterPreset->setEnabled(true);
		updateResultPanel("Bolt", c);
	});
	connect(m_controller, &MainController::searchFailed, this, [this, statusTime]() {
		ui->lbStatus->setText(statusTime() + "  |  Search Failed");
		m_hasSearchResult = false;
		ui->pbRegisterPreset->setEnabled(false);
		clearResultPanel();
	});

	connect(m_controller, &MainController::resultImageReady, this, [this](const QString& path) {
		QPixmap px(path);
		if (px.isNull()) return;
		m_imageViewer->setImage(px);
	});

	// Preset combo + Run button
	loadPresets();
	updateDemoProgressLabel();
	connect(ui->cbPreset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
		m_demoPointIdx      = 0;
		m_demoSearchedCount = 0;
		updateDemoProgressLabel();
	});
	connect(ui->pbRunPreset,       &QPushButton::clicked, this, &MainWindow::onRunPresetClicked);
	connect(ui->pbRegisterPreset,  &QPushButton::clicked, this, &MainWindow::onRegisterPresetClicked);
	connect(ui->pbAddPreset,        &QPushButton::clicked, this, &MainWindow::onAddPresetClicked);

	loadSettings();

	// Attach spdlog sink to log widget
	auto qtSink = std::make_shared<QtSpdlogSink_mt>(ui->leLog);
	qtSink->set_pattern("[%H:%M:%S.%e] %v");
	spdlog::default_logger()->sinks().push_back(qtSink);

	// Clock
	ui->lbClock->setTextFormat(Qt::RichText);
	ui->lbClock->setMinimumHeight(54);
	auto updateClock = [this]() {
		QString t = QTime::currentTime().toString("HH:mm:ss");
		QString d = QDate::currentDate().toString("yyyy.MM.dd");
		ui->lbClock->setText(
			QString("<div style='text-align:right;'>%1<br>"
					"<span style='font-size:8pt; color:rgba(100,100,100,210);'>%2</span></div>")
			.arg(t, d));
	};
	updateClock();
	auto* clockTimer = new QTimer(this);
	connect(clockTimer, &QTimer::timeout, this, updateClock);
	clockTimer->start(1000);
}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::onOpenButtonClicked()
{
	QString filePath = QFileDialog::getOpenFileName(this, tr("Open Point Cloud File"), "", tr("PCD Files (*.pcd)"));
	if (!filePath.isEmpty())
	{
		emit openFileRequested(filePath);
	}
}

QString MainWindow::droppedPointCloudPath(const QMimeData* mime)
{
	if (!mime || !mime->hasUrls())
		return {};

	for (const QUrl& url : mime->urls())
	{
		if (!url.isLocalFile())
			continue;

		const QString path = url.toLocalFile();
		if (path.endsWith(".pcd", Qt::CaseInsensitive) && QFileInfo(path).isFile())
			return path;
	}
	return {};
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
	if (droppedPointCloudPath(event->mimeData()).isEmpty())
	{
		QMainWindow::dragEnterEvent(event);
		return;
	}
	event->setDropAction(Qt::CopyAction);
	event->accept();
}

void MainWindow::dragMoveEvent(QDragMoveEvent* event)
{
	if (droppedPointCloudPath(event->mimeData()).isEmpty())
	{
		QMainWindow::dragMoveEvent(event);
		return;
	}
	event->setDropAction(Qt::CopyAction);
	event->accept();
}

void MainWindow::dropEvent(QDropEvent* event)
{
	const QString filePath = droppedPointCloudPath(event->mimeData());
	if (filePath.isEmpty())
	{
		QMainWindow::dropEvent(event);
		return;
	}

	event->setDropAction(Qt::CopyAction);
	event->accept();

	spdlog::info("[General] Dropped file: {}", filePath.toStdString());
	emit openFileRequested(filePath);
}

void MainWindow::loadSettings()
{
	QString iniPath = QCoreApplication::applicationDirPath() + "/setting.ini";
	QSettings s(iniPath, QSettings::IniFormat);

	s.beginGroup("Crop");
	ui->sbCropX->setValue(s.value("SizeX", 100).toInt());
	ui->sbCropY->setValue(s.value("SizeY", 100).toInt());
	ui->sbCropZ->setValue(s.value("SizeZ", 100).toInt());
	s.endGroup();

	s.beginGroup("BoltFinder");
	ui->dsbBoltFloorRemoval->setValue(s.value("FloorRemovalDist", 1.0).toDouble());
	ui->dsbBoltSurfaceLayer->setValue(s.value("SurfaceLayer", 2.0).toDouble());
	s.endGroup();

	s.beginGroup("HoleFinder");
	ui->dsbHoleRansacDist->setValue(s.value("RansacDistThreshold", 0.3).toDouble());
	s.endGroup();
}

void MainWindow::saveSettings()
{
	QString iniPath = QCoreApplication::applicationDirPath() + "/setting.ini";
	QSettings s(iniPath, QSettings::IniFormat);
	s.beginGroup("Crop");
	s.setValue("SizeX", ui->sbCropX->value());
	s.setValue("SizeY", ui->sbCropY->value());
	s.setValue("SizeZ", ui->sbCropZ->value());
	s.endGroup();

	s.beginGroup("BoltFinder");
	s.setValue("FloorRemovalDist", ui->dsbBoltFloorRemoval->value());
	s.setValue("SurfaceLayer",     ui->dsbBoltSurfaceLayer->value());
	s.endGroup();

	s.beginGroup("HoleFinder");
	s.setValue("RansacDistThreshold", ui->dsbHoleRansacDist->value());
	s.endGroup();

	spdlog::info("[Settings] Saved to {}", iniPath.toStdString());
}

void MainWindow::closeEvent(QCloseEvent* event)
{
	saveSettings();
	QMainWindow::closeEvent(event);
}

void MainWindow::loadPresets()
{
	m_presets.clear();
	ui->cbPreset->clear();

	const QString jsonPath = QCoreApplication::applicationDirPath() + "/presets.json";
	const QString iniPath  = QCoreApplication::applicationDirPath() + "/presets.ini";

	if (!QFileInfo::exists(jsonPath) && QFileInfo::exists(iniPath))
	{
		// --- migrate from old presets.ini ---
		spdlog::info("[Presets] Migrating presets.ini -> presets.json ...");
		QSettings s(iniPath, QSettings::IniFormat);
		for (const QString& group : s.childGroups())
		{
			s.beginGroup(group);
			PresetEntry preset;
			preset.name = s.value("Name", group).toString();
			int i = 1;
			while (s.contains(QString("Point%1_Type").arg(i)))
			{
				PointEntry pt;
				pt.filePath           = s.value(QString("Point%1_File").arg(i),                "").toString();
				pt.type               = s.value(QString("Point%1_Type").arg(i),           "Hole").toString();
				pt.cx                 = s.value(QString("Point%1_CropPosX").arg(i),          0.0).toFloat();
				pt.cy                 = s.value(QString("Point%1_CropPosY").arg(i),          0.0).toFloat();
				pt.cz                 = s.value(QString("Point%1_CropPosZ").arg(i),          0.0).toFloat();
				pt.sx                 = s.value(QString("Point%1_CropSizeX").arg(i),         100).toFloat();
				pt.sy                 = s.value(QString("Point%1_CropSizeY").arg(i),         100).toFloat();
				pt.sz                 = s.value(QString("Point%1_CropSizeZ").arg(i),         100).toFloat();
				pt.floorRemovalDist = s.value(QString("Point%1_MinHeightThreshold").arg(i), 1.0).toFloat();
				pt.tolerance        = s.value(QString("Point%1_CutOffTolerance").arg(i),    2.0).toFloat();
				preset.points.append(pt);
				++i;
			}
			s.endGroup();
			m_presets.append(preset);
		}
		saveAllPresets();
		QFile::rename(iniPath, iniPath + ".bak");
		spdlog::info("[Presets] Migration complete. Old file renamed to presets.ini.bak");
	}
	else if (!QFileInfo::exists(jsonPath))
	{
		// --- create default presets.json ---
		PresetEntry def;
		def.name = "Preset_01";
		m_presets.append(def);
		saveAllPresets();
		spdlog::info("[Presets] Created default presets.json");
	}
	else
	{
		// --- load from presets.json ---
		QFile f(jsonPath);
		if (!f.open(QIODevice::ReadOnly))
		{
			spdlog::error("[Presets] Cannot open presets.json for reading.");
		}
		else
		{
			const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
			f.close();
			for (const QJsonValue& v : doc.array())
			{
				const QJsonObject obj = v.toObject();
				PresetEntry preset;
				preset.name = obj["name"].toString();
				for (const QJsonValue& pv : obj["points"].toArray())
				{
					const QJsonObject po = pv.toObject();
					PointEntry pt;
					pt.filePath           = po["file"].toString();
					pt.type               = po["type"].toString("Hole");
					pt.cx                 = static_cast<float>(po["cx"].toDouble());
					pt.cy                 = static_cast<float>(po["cy"].toDouble());
					pt.cz                 = static_cast<float>(po["cz"].toDouble());
					pt.sx                 = static_cast<float>(po["sx"].toDouble(100));
					pt.sy                 = static_cast<float>(po["sy"].toDouble(100));
					pt.sz                 = static_cast<float>(po["sz"].toDouble(100));
						if (pt.type.compare("Hole", Qt::CaseInsensitive) == 0)
						{
							pt.tolerance        = static_cast<float>(po["planeFitTolerance"].toDouble(0.7));
							pt.floorRemovalDist = 0.0f;
						}
						else
						{
							pt.floorRemovalDist = static_cast<float>(po["floorRemovalDist"].toDouble(1.0));
							pt.tolerance        = static_cast<float>(po["surfaceLayer"].toDouble(2.0));
						}
					preset.points.append(pt);
				}
				m_presets.append(preset);
			}
		}
	}

	for (const PresetEntry& p : m_presets)
		ui->cbPreset->addItem(p.name);

	if (m_presets.isEmpty())
		ui->cbPreset->addItem("(No presets found)");
}

void MainWindow::runDemoSearch(const QString& presetName, const PointEntry& pt)
{
	spdlog::info("[Demo] Search '{}' ({})", presetName.toStdString(), pt.type.toStdString());

	emit cropRequested(pt.cx, pt.cy, pt.cz, pt.sx, pt.sy, pt.sz);
	ui->openGLWidget->setCropBox(pt.cx, pt.cy, pt.cz, pt.sx, pt.sy, pt.sz);
	ui->openGLWidget->focusOnBox(pt.cx, pt.cy, pt.cz, pt.sx, pt.sy, pt.sz);

	if (pt.type.compare("Hole", Qt::CaseInsensitive) == 0)
	{
		ui->dsbHoleRansacDist->setValue(static_cast<double>(pt.tolerance));
		emit holeSearchRequested(ui->dsbHoleRansacDist->value());
	}
	else
	{
		ui->dsbBoltFloorRemoval->setValue(static_cast<double>(pt.floorRemovalDist));
		ui->dsbBoltSurfaceLayer->setValue(static_cast<double>(pt.tolerance));
		BoltSearchParams p;
		p.floorRemovalDist = pt.floorRemovalDist;
		p.surfaceLayer     = pt.tolerance;
		emit boltSearchRequested(p);
	}
}

// Resolve a PointEntry file path to absolute
static QString resolveFilePath(const QString& path)
{
	if (path.isEmpty()) return path;
	if (QFileInfo(path).isRelative())
		return QCoreApplication::applicationDirPath() + "/" + path;
	return path;
}

void MainWindow::updateDemoProgressLabel()
{
	const int presetIdx = ui->cbPreset->currentIndex();
	if (presetIdx < 0 || presetIdx >= m_presets.size() || m_presets[presetIdx].points.isEmpty())
	{
		ui->lbDemoProgress->setText("-");
		return;
	}
	const int total = m_presets[presetIdx].points.size();
	ui->lbDemoProgress->setText(QString("%1/%2").arg(m_demoSearchedCount).arg(total));
}

void MainWindow::onRunPresetClicked()
{
	const int idx = ui->cbPreset->currentIndex();
	if (idx < 0 || idx >= m_presets.size())
	{
		spdlog::warn("[Demo] No preset selected or presets.ini not found.");
		return;
	}

	const PresetEntry& preset = m_presets[idx];
	if (preset.points.isEmpty())
	{
		spdlog::warn("[Demo] Preset '{}' has no registered points.", preset.name.toStdString());
		return;
	}

	const int ptIdx = m_demoPointIdx % preset.points.size();
	const PointEntry& pt = preset.points[ptIdx];
	const QString filePath = resolveFilePath(pt.filePath);

	if (filePath.isEmpty() || !QFileInfo::exists(filePath))
	{
		spdlog::error("[Demo] File not found for Point{}: '{}'", ptIdx + 1, filePath.toStdString());
		return;
	}

	auto moveCameraAndAdvance = [&]()
	{
		// Phase 1 body: set crop params and move camera
		ui->dsbCropPosX->setValue(static_cast<double>(pt.cx));
		ui->dsbCropPosY->setValue(static_cast<double>(pt.cy));
		ui->dsbCropPosZ->setValue(static_cast<double>(pt.cz));
		ui->sbCropX->setValue(static_cast<int>(pt.sx));
		ui->sbCropY->setValue(static_cast<int>(pt.sy));
		ui->sbCropZ->setValue(static_cast<int>(pt.sz));
		ui->openGLWidget->setCropBox(pt.cx, pt.cy, pt.cz, pt.sx, pt.sy, pt.sz);
		ui->openGLWidget->focusOnBox(pt.cx, pt.cy, pt.cz, pt.sx, pt.sy, pt.sz);

		++m_demoSearchedCount;
		updateDemoProgressLabel();
		spdlog::info("[Demo] '{}' Point{} - camera moved, click again to search.", preset.name.toStdString(), ptIdx + 1);
		m_demoPhase = 2;
	};

	if (m_demoPhase == 0)
	{
		// Phase 0: load scene
		// New file: emit load, show full PC (no crop box), wait for next click
		// Same file: skip straight to camera-move phase
		if (filePath != m_controller->loadedFilePath())
		{
			spdlog::info("[Demo] '{}' Point{} - loading file...", preset.name.toStdString(), ptIdx + 1);
			emit openFileRequested(filePath);
			ui->openGLWidget->hideCropBox();
			m_demoPhase = 1;
		}
		else
		{
			spdlog::info("[Demo] '{}' Point{} - same file, moving camera...", preset.name.toStdString(), ptIdx + 1);
			moveCameraAndAdvance();
		}
	}
	else if (m_demoPhase == 1)
	{
		// Phase 1: file just loaded - set crop params and move camera
		moveCameraAndAdvance();
	}
	else
	{
		// Phase 2: run search
		if (pt.type.compare("Hole", Qt::CaseInsensitive) == 0)
			spdlog::info("[Demo] '{}' Point{} - searching. (Plane Fit Tolerance: {:.3f}mm)", preset.name.toStdString(), ptIdx + 1, pt.tolerance);
		else
			spdlog::info("[Demo] '{}' Point{} - searching. (Floor Removal Dist: {:.1f}mm, Surface Layer: {:.3f}mm)", preset.name.toStdString(), ptIdx + 1, pt.floorRemovalDist, pt.tolerance);
		runDemoSearch(preset.name, pt);

		++m_demoPointIdx;

		if (m_demoPointIdx >= preset.points.size())
		{
			m_demoPointIdx      = 0;
			m_demoSearchedCount = 0;
			updateDemoProgressLabel();
			m_demoPhase         = 0;
		}
		else
		{
			m_demoPhase = 0;
		}
	}
}

// -- saveAllPresets -------------------------------------------

void MainWindow::saveAllPresets()
{
	QJsonArray root;
	for (const PresetEntry& preset : m_presets)
	{
		QJsonArray pts;
		for (const PointEntry& pt : preset.points)
		{
			QJsonObject po;
				po["file"] = pt.filePath;
				po["type"] = pt.type;
				po["cx"]   = static_cast<double>(pt.cx);
				po["cy"]   = static_cast<double>(pt.cy);
				po["cz"]   = static_cast<double>(pt.cz);
				po["sx"]   = static_cast<double>(pt.sx);
				po["sy"]   = static_cast<double>(pt.sy);
				po["sz"]   = static_cast<double>(pt.sz);
				if (pt.type.compare("Hole", Qt::CaseInsensitive) == 0)
				{
					po["planeFitTolerance"] = static_cast<double>(pt.tolerance);
				}
				else
				{
					po["floorRemovalDist"] = static_cast<double>(pt.floorRemovalDist);
					po["surfaceLayer"]     = static_cast<double>(pt.tolerance);
				}
			pts.append(po);
		}
		QJsonObject obj;
		obj["name"]   = preset.name;
		obj["points"] = pts;
		root.append(obj);
	}

	const QString jsonPath = QCoreApplication::applicationDirPath() + "/presets.json";
	QFile f(jsonPath);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
	{
		spdlog::error("[Presets] Cannot write presets.json: {}", jsonPath.toStdString());
		return;
	}
	f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
	f.close();
}

// -- onRegisterPresetClicked ----------------------------------

void MainWindow::onRegisterPresetClicked()
{
	if (!m_hasSearchResult)
	{
		spdlog::warn("[Preset] Register skipped: no search result available.");
		return;
	}

	const int idx = ui->cbPreset->currentIndex();
	if (idx < 0 || idx >= m_presets.size())
	{
		spdlog::warn("[Preset] Register skipped: no preset selected.");
		return;
	}

	PresetEntry& preset = m_presets[idx];

	PointEntry pt;
	pt.filePath           = m_controller->loadedFilePath();
	pt.type               = m_lastSearchType;
	pt.cx                 = static_cast<float>(ui->dsbCropPosX->value());
	pt.cy                 = static_cast<float>(ui->dsbCropPosY->value());
	pt.cz                 = static_cast<float>(ui->dsbCropPosZ->value());
	pt.sx                 = static_cast<float>(ui->sbCropX->value());
	pt.sy                 = static_cast<float>(ui->sbCropY->value());
	pt.sz                 = static_cast<float>(ui->sbCropZ->value());
	if (m_lastSearchType.compare("Hole", Qt::CaseInsensitive) == 0)
	{
		pt.tolerance        = static_cast<float>(ui->dsbHoleRansacDist->value());
		pt.floorRemovalDist = 0.0f;
	}
	else
	{
		pt.floorRemovalDist = static_cast<float>(ui->dsbBoltFloorRemoval->value());
		pt.tolerance        = static_cast<float>(ui->dsbBoltSurfaceLayer->value());
	}

	preset.points.append(pt);
	saveAllPresets();

	spdlog::info("[Preset] '{}' Point{} added | Type={} | File={} | Pos=({:.2f},{:.2f},{:.2f}) | Size=({:.0f},{:.0f},{:.0f})",
		preset.name.toStdString(), preset.points.size(),
		pt.type.toStdString(),
		pt.filePath.toStdString(),
		pt.cx, pt.cy, pt.cz,
		pt.sx, pt.sy, pt.sz);
	ui->lbStatus->setText(QTime::currentTime().toString("HH:mm:ss") + "  |  Preset Registered");
	updateDemoProgressLabel();
}

// -- onAddPresetClicked ----------------------------------

void MainWindow::onAddPresetClicked()
{
	QStringList existing;
	for (const PresetEntry& p : m_presets)
		existing.append(p.name);

	int n = existing.size() + 1;
	QString newName;
	do {
		newName = QString("Preset_%1").arg(n, 2, 10, QChar('0'));
		++n;
	} while (existing.contains(newName));

	PresetEntry preset;
	preset.name = newName;
	m_presets.append(preset);
	saveAllPresets();

	ui->cbPreset->addItem(newName);
	ui->cbPreset->setCurrentIndex(ui->cbPreset->count() - 1);

	spdlog::info("[Preset] New preset '{}' created.", newName.toStdString());
	ui->lbStatus->setText(QTime::currentTime().toString("HH:mm:ss") + "  |  Preset '" + newName + "' Created");
}

// -- Result Panel ---------------------------------------------

void MainWindow::updateResultPanel(const QString& type, const Circle3D& c)
{
	const QString typeStyle   = (type == "Hole")
		? "color:#27ae60; font-weight:bold;"   // green
		: "color:#2980b9; font-weight:bold;";  // blue

	ui->lbResultType->setText(type);
	ui->lbResultType->setStyleSheet(typeStyle);

	ui->lbResultCenter->setText(QString("(%1, %2, %3)")
		.arg(static_cast<double>(c.center.x), 0, 'f', 2)
		.arg(static_cast<double>(c.center.y), 0, 'f', 2)
		.arg(static_cast<double>(c.center.z), 0, 'f', 2));

	ui->lbResultRadius->setText(QString("%1 mm")
		.arg(static_cast<double>(c.radius), 0, 'f', 3));

	ui->lbResultNormal->setText(QString("(%1, %2, %3)")
		.arg(static_cast<double>(c.normal[0]), 0, 'f', 3)
		.arg(static_cast<double>(c.normal[1]), 0, 'f', 3)
		.arg(static_cast<double>(c.normal[2]), 0, 'f', 3));

	ui->lbResultPlaneCenter->setText(QString("(%1, %2, %3)")
		.arg(static_cast<double>(c.planeCenter.x), 0, 'f', 2)
		.arg(static_cast<double>(c.planeCenter.y), 0, 'f', 2)
		.arg(static_cast<double>(c.planeCenter.z), 0, 'f', 2));
}

void MainWindow::clearResultPanel()
{
	ui->lbResultType->setText("-");
	ui->lbResultType->setStyleSheet("");
	ui->lbResultCenter->setText("-");
	ui->lbResultRadius->setText("-");
	ui->lbResultPlaneCenter->setText("-");
	ui->lbResultNormal->setText("-");
}
