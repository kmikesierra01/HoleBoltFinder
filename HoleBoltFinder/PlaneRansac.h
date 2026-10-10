#pragma once
#include "Types.h"
#include <optional>

/**
 * @brief Finds the dominant plane in an organized point cloud (CV_32FC3) with RANSAC
 *
 * References:
 *   - M. A. Fischler, R. C. Bolles, "Random Sample Consensus", CACM 24(6), 1981
 *   - R. Hartley, A. Zisserman, "Multiple View Geometry", 2nd ed., Sec. 4.7.1
 *     (re-estimating the model from all inliers of the best consensus set)
 *
 * Usage:
 *   PlaneRansac ransac;
 *   ransac.setInput(pc);
 *   auto plane = ransac.execute();         // std::optional<Plane>
 *   cv::Mat inliers = ransac.getInliers(); // same size as input, non-inliers = 0
 */
class PlaneRansac
{
public:
    void setInput(const cv::Mat& pointCloud) { m_input = pointCloud; }
    void setIterations(int n)                { m_iterations = n; }
    void setInlierDistance(double mm)        { m_inlierDistance = mm; }
    void setMinInliers(int n)                { m_minInliers = n; }

    std::optional<Plane> execute() const;
    cv::Mat getInliers() const { return m_inliers; }

private:
    cv::Mat m_input;
    int     m_iterations     = 500;
    double  m_inlierDistance = 0.3;
    int     m_minInliers     = 1000;
    mutable cv::Mat m_inliers;
};
