#pragma once

#include "Types.h"
#include "HoleFinder.h"       // reuse fitCircle
#include "PlaneRansac.h"
#include <optional>
#include <vector>

/**
 * @brief Finds a bolt (protrusion) in a 3D point cloud
 *
 * Algorithm:
 *   1. PlaneRansac - find reference plane
 *   2. Remove the floor; keep the dominant layer above it (height histogram peak)
 *   3. Statistical outlier removal (kNN-based)
 *   4. Remaining points -> 2D binary image
 *   5. Morphological noise removal
 *   6. Find largest external contour -> extract 3D points -> Kasa circle fit
 *
 * Usage:
 *   BoltFinder finder;
 *   finder.setPointCloud(croppedMat);  // CV_32FC3
 *   auto result = finder.execute();     // std::optional<Circle3D>
 *   Plane debugPlane = finder.getDebugPlane();  // RANSAC plane (for debug)
 */
class BoltFinder
{
public:
    BoltFinder() = default;

    void    setPointCloud(const cv::Mat& pc) { m_pc = pc; }
    cv::Mat getPointCloud() const { return m_pc; }

    /** Minimum height above plane to consider (mm) - points below this are treated as floor */
    void  setFloorRemovalDist(float v) { m_floorRemovalDist = v; }
    float getFloorRemovalDist() const  { return m_floorRemovalDist; }

    /** Margin allowed when filtering (unit: mm) */
    void  setSurfaceLayer(float v) { m_surfaceLayer = v; }
    float getSurfaceLayer() const  { return m_surfaceLayer; }

    /** Number of outlier removal iterations */
    void setOutlierIterations(int v)   { m_outlierIterations = v; }
    int  getOutlierIterations() const  { return m_outlierIterations; }

    /** Number of kNN neighbors for outlier removal */
    void setOutlierKNeighbors(int v)   { m_outlierKNeighbors = v; }
    int  getOutlierKNeighbors() const  { return m_outlierKNeighbors; }

    /** Standard deviation multiplier for outlier removal */
    void  setOutlierStdDevMult(float v) { m_outlierStdDevMult = v; }
    float getOutlierStdDevMult() const  { return m_outlierStdDevMult; }

    /** [Debug] Returns the reference plane found by RANSAC */
    Plane getDebugPlane() const { return m_debugPlane; }

    /** @return Circle3D (center, normal, radius), or std::nullopt on failure */
    std::optional<Circle3D> execute() const;

    /** Result visualization image (BGR) generated after execute() */
    cv::Mat getResultImage() const { return m_resultImage; }

private:
    cv::Mat keepTopLayer(const cv::Mat& pc, const Plane& plane) const;
    cv::Mat removeOutliers(const cv::Mat& pc) const;
    cv::Mat statisticalOutlierPass(const cv::Mat& pc, int kNeighbors, float stdDevMult) const;
    cv::Mat toMask(const cv::Mat& pc) const;
    cv::Mat openClose(const cv::Mat& src, int kernelSize) const;
    bool    pointAt(const cv::Mat& pc, cv::Point px, int range, cv::Point3f& out) const;

    cv::Mat m_pc;
    float   m_floorRemovalDist  = 1.0f;
    float   m_surfaceLayer      = 2.0f;
    int     m_outlierIterations = 2;
    int     m_outlierKNeighbors = 20;
    float   m_outlierStdDevMult = 2.0f;
    mutable Plane   m_debugPlane;
    mutable cv::Mat m_resultImage;
};
