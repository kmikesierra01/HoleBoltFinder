#pragma once

#include <QObject>
#include <optional>
#include "Types.h"

class MainController : public QObject
{
	Q_OBJECT

public:
	MainController(QObject* parent = nullptr);
	~MainController();

	const PCData* currentPointCloud() const;

signals:
	void pointCloudLoaded(const PCData& pc);
	void cropDone(int pointCount);
	void holeFound(const Circle3D& circle);
	void boltFound(const Circle3D& circle);
	void searchFailed();
	void resultImageReady(const QString& imagePath);

public slots:
	void onOpenFileRequested(const QString& filePath);
	void onCropRequested(float cx, float cy, float cz, float sx, float sy, float sz);
	void onHoleSearchRequested(double distThreshold);
	void onBoltSearchRequested(BoltSearchParams params);

	const QString& loadedFilePath() const { return m_loadedFilePath; }

private:
	std::optional<PCData> m_pointCloud;
	std::optional<PCData> m_currentCloud;
	QString m_loadedFilePath;
	bool m_isCropped = false;
};
