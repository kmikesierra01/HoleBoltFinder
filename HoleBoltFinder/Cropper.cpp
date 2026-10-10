#include "pch.h"
#include "Cropper.h"
#include <algorithm>
#include <limits>

// -- execute --------------------------------------------------

std::optional<CroppedData> Cropper::execute() const
{
    if (m_pc.empty() || m_pc.type() != CV_32FC3)
    {
        spdlog::error("[Crop] Invalid point cloud.");
        return std::nullopt;
    }
    if (m_size.x <= 0 || m_size.y <= 0 || m_size.z <= 0)
    {
        spdlog::error("[Crop] Invalid box size.");
        return std::nullopt;
    }

    // Compute 2D ROI region from pure 3D bounds
    const float minX = m_center.x - m_size.x * 0.5f, maxX = m_center.x + m_size.x * 0.5f;
    const float minY = m_center.y - m_size.y * 0.5f, maxY = m_center.y + m_size.y * 0.5f;
    const float minZ = m_center.z - m_size.z * 0.5f, maxZ = m_center.z + m_size.z * 0.5f;

    int r0 = m_pc.rows, r1 = -1, c0 = m_pc.cols, c1 = -1;

    for (int r = 0; r < m_pc.rows; ++r)
        for (int c = 0; c < m_pc.cols; ++c)
        {
            const auto& v = m_pc.at<cv::Vec3f>(r, c);
            if (!isValidPoint(v)) continue;
            if (v[0] >= minX && v[0] <= maxX &&
                v[1] >= minY && v[1] <= maxY &&
                v[2] >= minZ && v[2] <= maxZ)
            {
                r0 = std::min(r0, r); r1 = std::max(r1, r);
                c0 = std::min(c0, c); c1 = std::max(c1, c);
            }
        }

    if (r1 < r0 || c1 < c0)
    {
        spdlog::warn("[Crop] No points found inside the 3D box.");
        return std::nullopt;
    }

    CroppedData data;

    // Unorganized cloud (height == 1): rasterize by XY world coordinates into a 2D grid
    if (m_pc.rows == 1)
    {
        // Collect valid points inside the box
        std::vector<cv::Vec3f> pts;
        for (int c = 0; c < m_pc.cols; ++c)
        {
            const auto& v = m_pc.at<cv::Vec3f>(0, c);
            if (!isValidPoint(v)) continue;
            if (v[0] >= minX && v[0] <= maxX &&
                v[1] >= minY && v[1] <= maxY &&
                v[2] >= minZ && v[2] <= maxZ)
                pts.push_back(v);
        }
        if (pts.empty())
        {
            spdlog::warn("[Crop] No points inside box.");
            return std::nullopt;
        }

        // Build a 2D grid: X -> col, Y -> row
        // Resolution: aim for a square-ish image, max 1mm per pixel
        const float rangeX = maxX - minX;
        const float rangeY = maxY - minY;
        const float resolution = std::min({ rangeX, rangeY, 1.0f });
        const int gridW = std::max(10, static_cast<int>(std::ceil(rangeX / resolution)));
        const int gridH = std::max(10, static_cast<int>(std::ceil(rangeY / resolution)));

        const float kNaN = std::numeric_limits<float>::quiet_NaN();
        data.pointCloud = cv::Mat(gridH, gridW, CV_32FC3, cv::Scalar(kNaN, kNaN, kNaN));

        for (const auto& v : pts)
        {
            const int col = static_cast<int>((v[0] - minX) / resolution);
            const int row = static_cast<int>((v[1] - minY) / resolution);
            if (col >= 0 && col < gridW && row >= 0 && row < gridH)
            {
                auto& cell = data.pointCloud.at<cv::Vec3f>(row, col);
                // Keep the point with highest Z (closest to camera / top of surface)
                if (!isValidPoint(cell) || v[2] > cell[2])
                    cell = v;
            }
        }
        spdlog::debug("[Crop] Rasterized unorganized cloud: {}x{} grid ({} pts)", gridW, gridH, pts.size());
        return data;
    }

    cv::Rect roi(c0, r0, c1 - c0 + 1, r1 - r0 + 1);
    CroppedData result;
    result.pointCloud = m_pc(roi).clone();
    if (!m_texture.empty() && m_texture.size() == m_pc.size())
        result.texture = m_texture(roi).clone();

    // Remove points outside 3D bounds within the cropped ROI (handles non-rectangular ROI)
    filterWith3DBound(result);
    return result;
}

// -- Private ---------------------------------------------------

void Cropper::filterWith3DBound(CroppedData& data) const
{
    const float minX = m_center.x - m_size.x * 0.5f, maxX = m_center.x + m_size.x * 0.5f;
    const float minY = m_center.y - m_size.y * 0.5f, maxY = m_center.y + m_size.y * 0.5f;
    const float minZ = m_center.z - m_size.z * 0.5f, maxZ = m_center.z + m_size.z * 0.5f;

    for (int r = 0; r < data.pointCloud.rows; ++r)
        for (int c = 0; c < data.pointCloud.cols; ++c)
        {
            auto& v = data.pointCloud.at<cv::Vec3f>(r, c);
            if (!isValidPoint(v)) continue;
            if (v[0] < minX || v[0] > maxX ||
                v[1] < minY || v[1] > maxY ||
                v[2] < minZ || v[2] > maxZ)
            {
                v = cv::Vec3f(0, 0, 0);
                if (!data.texture.empty())
                {
                    if (data.texture.channels() == 1)
                        data.texture.at<uchar>(r, c) = 0;
                    else
                        data.texture.at<cv::Vec3b>(r, c) = cv::Vec3b(0, 0, 0);
                }
            }
        }
}
