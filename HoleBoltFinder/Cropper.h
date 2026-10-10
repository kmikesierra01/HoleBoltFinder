#pragma once

#include "Types.h"
#include <optional>

/**
 * @brief Independent class for cropping a 3D point cloud with a 3D box (center + size)
 *
 * Algorithm:
 *   1. Compute 3D coordinates of the 4 box corners
 *   2. Find the nearest 2D pixel for each corner (linear search)
 *   3. Crop ROI from center of 4 corner 2D coords + padding
 *   4. Secondary filter by 3D bounds within the cropped ROI
 *
 * Usage:
 *   Cropper cropper;
 *   cropper.setInputPointCloud(mat);
 *   cropper.setCenter(center);
 *   cropper.setSize(size);
 *   auto result = cropper.execute();  // optional<CroppedData>
 */
class Cropper
{
public:
    Cropper() = default;

    void setInputPointCloud(const cv::Mat& pc)    { m_pc = pc; }
    void setInputTexture(const cv::Mat& tex)      { m_texture = tex; }
    void setCenter(const cv::Point3f& center)     { m_center = center; }
    void setSize(const cv::Point3f& size)         { m_size = size; }

    cv::Mat getInputPointCloud() const { return m_pc; }

    std::optional<CroppedData> execute() const;

private:
    void filterWith3DBound(CroppedData& data) const;

    cv::Mat      m_pc;
    cv::Mat      m_texture;
    cv::Point3f  m_center;
    cv::Point3f  m_size;
};
