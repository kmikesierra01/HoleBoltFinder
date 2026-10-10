#pragma once

#include "Types.h"
#include <optional>
#include <vector>
#include <memory>

/**
 * @brief Independent class for finding holes in a 3D point cloud
 *
 * Algorithm:
 *   1. Convert CV_32FC3 point cloud to a 2D binary image based on Z validity
 *   2. Morphological noise removal
 *   3. Find the largest inner hole from hierarchical contour list
 *   4. Extract 3D points from 2D contour, then Kasa least-squares circle fitting
 *
 * Usage:
 *   HoleFinder finder;
 *   finder.setPointCloud(croppedMat);  // CV_32FC3
 *   auto result = finder.execute();     // std::optional<Circle3D>
 */
class HoleFinder
{
public:
    HoleFinder() = default;

    void     setPointCloud(const cv::Mat& pc) { m_pc = pc; }
    cv::Mat  getPointCloud() const { return m_pc; }

    /** @return Circle3D (center, normal, radius), or std::nullopt on failure */
    std::optional<Circle3D> execute() const;

    /** Result visualization image (BGR) generated after execute() */
    cv::Mat getResultImage() const { return m_resultImage; }

    /** Kasa least-squares 3D circle fitting (can be used externally) */
    static std::optional<Circle3D> fitCircle(const std::vector<cv::Point3f>& pts);

private:
    bool openClose(const cv::Mat& src, int kernelSize, cv::Mat& dst) const;
    bool toMask(const cv::Mat& pc, cv::Mat& dst) const;
    bool findLargestInnerHole(const std::vector<std::vector<cv::Point>>& contours,
                              std::vector<cv::Point>& out) const;
    bool pointAt(const cv::Mat& pc, cv::Point px, int range, cv::Point3f& out) const;

    cv::Mat m_pc;
    mutable cv::Mat m_resultImage;
};