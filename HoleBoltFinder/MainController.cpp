#include "pch.h"
#include "MainController.h"
#include "PointCloudHandler.h"
#include "Cropper.h"
#include "PlaneRansac.h"
#include "HoleFinder.h"
#include "BoltFinder.h"

MainController::MainController(QObject* parent)
	: QObject(parent)
{}

MainController::~MainController()
{}

const PCData* MainController::currentPointCloud() const
{
	return m_pointCloud ? &(*m_pointCloud) : nullptr;
}

void MainController::onOpenFileRequested(const QString& filePath)
{
	if (filePath == m_loadedFilePath)
	{
		spdlog::info("[General] File already loaded, skipping: {}", filePath.toStdString());
		return;
	}

	spdlog::info("[General] Loading file: {}", filePath.toStdString());

	auto result = PointCloudHandler::buildFromPCD(filePath);
	if (!result)
	{
		spdlog::error("[General] Failed to load point cloud: {}", filePath.toStdString());
		return;
	}

	m_loadedFilePath = filePath;
	m_pointCloud = std::move(result);
	m_currentCloud = m_pointCloud;
	m_isCropped = false;
	emit pointCloudLoaded(*m_pointCloud);
}

void MainController::onCropRequested(float cx, float cy, float cz, float sx, float sy, float sz)
{
	if (!m_pointCloud)
	{
		spdlog::warn("[Crop] No point cloud loaded");
		return;
	}

	Cropper cropper;
	cropper.setInputPointCloud(m_pointCloud->matPointCloud);
	cropper.setCenter(cv::Point3f(cx, cy, cz));
	cropper.setSize(cv::Point3f(sx, sy, sz));

	auto result = cropper.execute();
	if (!result) { spdlog::error("[Crop] Crop failed"); emit searchFailed(); return; }

	auto cropped = PointCloudHandler::fromCropped(*result);
	if (!cropped) { spdlog::warn("[Crop] Crop result empty"); emit searchFailed(); return; }

	spdlog::info("[Crop] Done: {} points (Min Z={:.2f}, Max Z={:.2f})", cropped->pointCount, cropped->minZ, cropped->maxZ);
	m_currentCloud = cropped;
	m_isCropped = true;
	emit cropDone(cropped->pointCount);
}

void MainController::onHoleSearchRequested(double distThreshold)
{
	if (!m_isCropped)
	{
		spdlog::warn("[Hole] Crop not done. Please crop first.");
		emit searchFailed();
		return;
	}

	spdlog::info("[Hole] Starting RANSAC plane finding...");

	PlaneRansac ransac;
	ransac.setInput(m_currentCloud->matPointCloud);
	ransac.setInlierDistance(distThreshold);
	ransac.setMinInliers(100);

	auto plane = ransac.execute();
	if (!plane)
	{
		spdlog::error("[Hole] RANSAC plane finding failed");
		emit searchFailed();
		return;
	}

	spdlog::info("[Hole] Plane found. Starting hole finding...");
	{
		cv::Vec3f ax_(plane->xEnd.x-plane->origin.x,plane->xEnd.y-plane->origin.y,plane->xEnd.z-plane->origin.z);
		cv::Vec3f ay_(plane->yEnd.x-plane->origin.x,plane->yEnd.y-plane->origin.y,plane->yEnd.z-plane->origin.z);
		cv::Vec3f pn_=ax_.cross(ay_); float pnL=(float)cv::norm(pn_); if(pnL>1e-9f) pn_/=pnL;
		spdlog::info("[Hole] RANSAC Plane: Center=({:.3f},{:.3f},{:.3f}), Normal=({:.3f},{:.3f},{:.3f})",
			plane->origin.x,plane->origin.y,plane->origin.z,pn_[0],pn_[1],pn_[2]);
	}

	HoleFinder holeFinder;
	holeFinder.setPointCloud(ransac.getInliers());

	auto circle = holeFinder.execute();
	if (!circle)
	{
		spdlog::error("[Hole] Hole finding failed");
		emit searchFailed();
		return;
	}

	spdlog::info("[Hole] Hole found: center=({:.3f}, {:.3f}, {:.3f}), R={:.3f}mm",
		circle->center.x, circle->center.y, circle->center.z, circle->radius);

		circle->planeCenter = cv::Point3f(
		static_cast<float>(plane->origin.x),
		static_cast<float>(plane->origin.y),
		static_cast<float>(plane->origin.z));
	{
		cv::Vec3f ax(plane->xEnd.x - plane->origin.x, plane->xEnd.y - plane->origin.y, plane->xEnd.z - plane->origin.z);
		cv::Vec3f ay(plane->yEnd.x - plane->origin.x, plane->yEnd.y - plane->origin.y, plane->yEnd.z - plane->origin.z);
		cv::Vec3f pn = ax.cross(ay);
		double pnLen = cv::norm(pn);
		if (pnLen > 1e-9) circle->planeNormal = cv::Vec3f(pn / static_cast<float>(pnLen));
	}
		emit holeFound(*circle);
	emit resultImageReady("debug/04_contours.png");
}

void MainController::onBoltSearchRequested(BoltSearchParams params)
{
	if (!m_isCropped)
	{
		spdlog::warn("[Bolt] Crop not done. Please crop first.");
		emit searchFailed();
		return;
	}

	spdlog::info("[Bolt] Starting bolt detection (RANSAC + outlier filter)...");

	BoltFinder finder;
	finder.setPointCloud(m_currentCloud->matPointCloud);
	finder.setFloorRemovalDist(params.floorRemovalDist);
	finder.setSurfaceLayer(params.surfaceLayer);
	spdlog::info("[Bolt] floorRemovalDist={:.3f}mm surfaceLayer={:.3f}mm", params.floorRemovalDist, params.surfaceLayer);

	auto circle = finder.execute();
	if (!circle)
	{
		spdlog::error("[Bolt] Bolt detection failed");
		emit searchFailed();
		return;
	}

	{
		Plane dp_=finder.getDebugPlane();
		cv::Vec3f ax2_(dp_.xEnd.x-dp_.origin.x,dp_.xEnd.y-dp_.origin.y,dp_.xEnd.z-dp_.origin.z);
		cv::Vec3f ay2_(dp_.yEnd.x-dp_.origin.x,dp_.yEnd.y-dp_.origin.y,dp_.yEnd.z-dp_.origin.z);
		cv::Vec3f pn2_=ax2_.cross(ay2_); float pn2L=(float)cv::norm(pn2_); if(pn2L>1e-9f) pn2_/=pn2L;
		spdlog::info("[Bolt] RANSAC Plane: Center=({:.3f},{:.3f},{:.3f}), Normal=({:.3f},{:.3f},{:.3f})",
			dp_.origin.x,dp_.origin.y,dp_.origin.z,pn2_[0],pn2_[1],pn2_[2]);
	}
	spdlog::info("[Bolt] Bolt found: center=({:.3f}, {:.3f}, {:.3f}), R={:.3f}, H={:.3f}mm",
		circle->center.x, circle->center.y, circle->center.z, circle->radius, circle->height);

		{
		Plane dp = finder.getDebugPlane();
		circle->planeCenter = cv::Point3f(dp.origin.x, dp.origin.y, dp.origin.z);
		cv::Vec3f ax2(dp.xEnd.x - dp.origin.x, dp.xEnd.y - dp.origin.y, dp.xEnd.z - dp.origin.z);
		cv::Vec3f ay2(dp.yEnd.x - dp.origin.x, dp.yEnd.y - dp.origin.y, dp.yEnd.z - dp.origin.z);
		cv::Vec3f pn2 = ax2.cross(ay2);
		double pn2Len = cv::norm(pn2);
		if (pn2Len > 1e-9) circle->planeNormal = cv::Vec3f(pn2 / static_cast<float>(pn2Len));
	}
		emit boltFound(*circle);
	emit resultImageReady("debug/bolt_05_contours.png");
}