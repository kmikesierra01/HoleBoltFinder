#include "pch.h"
#include "PlaneRansac.h"
#include <algorithm>
#include <cmath>
#include <random>

// Least-squares plane through the points: centroid + direction of least variance (PCA)
static Plane fitPlane(const std::vector<cv::Vec3f>& pts)
{
    cv::PCA pca(cv::Mat(pts).reshape(1), cv::Mat(), cv::PCA::DATA_AS_ROW);
    const cv::Vec3f n = cv::normalize(cv::Vec3f(pca.eigenvectors.row(2)));
    const cv::Point3f c(pca.mean.at<float>(0), pca.mean.at<float>(1), pca.mean.at<float>(2));

    // In-plane unit axes: a reference direction not parallel to n, projected onto the plane
    const cv::Vec3f ref = std::abs(n[0]) > 0.9f ? cv::Vec3f(0.0f, 1.0f, 0.0f) : cv::Vec3f(1.0f, 0.0f, 0.0f);
    const cv::Vec3f x = cv::normalize(ref - n * ref.dot(n));
    const cv::Vec3f y = n.cross(x);
    return { c, c + cv::Point3f(x), c + cv::Point3f(y) };
}

std::optional<Plane> PlaneRansac::execute() const
{
    m_inliers.release();

    if (m_input.empty() || m_input.type() != CV_32FC3)
    {
        spdlog::error("[RANSAC] Input must be a non-empty CV_32FC3 point cloud");
        return std::nullopt;
    }

    // Valid points + their pixel positions (to rebuild the inlier mat)
    std::vector<cv::Vec3f> pts;
    std::vector<cv::Point> pix;
    for (int r = 0; r < m_input.rows; ++r)
    {
        const cv::Vec3f* row = m_input.ptr<cv::Vec3f>(r);
        for (int c = 0; c < m_input.cols; ++c)
            if (isValidPoint(row[c])) { pts.push_back(row[c]); pix.emplace_back(c, r); }
    }

    const int n = static_cast<int>(pts.size());
    if (n < 3)
    {
        spdlog::error("[RANSAC] Not enough valid points: {}", n);
        return std::nullopt;
    }

    spdlog::trace("[RANSAC] {} valid pts, up to {} samples, inlier dist {}mm, need {} inliers",
        n, m_iterations, m_inlierDistance, m_minInliers);

    const float thr = static_cast<float>(m_inlierDistance);
    auto isInlier = [thr](const cv::Vec3f& normal, float d, const cv::Vec3f& p) {
        return std::abs(normal.dot(p) + d) < thr;
    };

    // Fixed seed: the same input always gives the same plane
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> pick(0, n - 1);

    cv::Vec3f bestNormal(0.0f, 0.0f, 1.0f);
    float bestD = 0.0f;
    int bestCount = 0;

    for (int it = 0; it < m_iterations; ++it)
    {
        // Minimal sample: 3 distinct points -> plane hypothesis
        const int i0 = pick(rng);
        int i1 = pick(rng);
        while (i1 == i0) i1 = pick(rng);
        int i2 = pick(rng);
        while (i2 == i0 || i2 == i1) i2 = pick(rng);

        const cv::Vec3f cross = (pts[i1] - pts[i0]).cross(pts[i2] - pts[i0]);
        const float len = static_cast<float>(cv::norm(cross));
        if (len < 1e-6f) continue;  // collinear sample

        const cv::Vec3f normal = cross / len;
        const float d = -normal.dot(pts[i0]);

        int count = 0;
        for (const auto& p : pts)
            if (isInlier(normal, d, p)) ++count;
        if (count <= bestCount) continue;

        bestNormal = normal;
        bestD = d;
        bestCount = count;
    }

    spdlog::trace("[RANSAC] best inliers = {} / {}", bestCount, n);

    if (bestCount < std::max(3, m_minInliers))
    {
        spdlog::error("[RANSAC] No plane: best sample has {} inliers, need {}", bestCount, m_minInliers);
        return std::nullopt;
    }

    // Least-squares refit (PCA) on the consensus set, then re-select inliers against the
    // refitted plane and refit once more. This removes most of the noise of the 3-point sample.
    std::vector<int> inlierIdx;
    Plane plane;
    for (int pass = 0; pass < 2; ++pass)
    {
        inlierIdx.clear();
        for (int i = 0; i < n; ++i)
            if (isInlier(bestNormal, bestD, pts[i])) inlierIdx.push_back(i);
        if (inlierIdx.size() < 3) break;

        std::vector<cv::Vec3f> inliers;
        inliers.reserve(inlierIdx.size());
        for (int i : inlierIdx) inliers.push_back(pts[i]);

        plane = fitPlane(inliers);
        bestNormal = planeNormal(plane);
        bestD = -bestNormal.dot(cv::Vec3f(plane.origin));
    }
    if (inlierIdx.size() < 3)
    {
        spdlog::error("[RANSAC] FAIL: refit left {} inliers", inlierIdx.size());
        return std::nullopt;
    }

    // Inlier mat (same size as input, non-inliers = 0) for downstream finders
    m_inliers = cv::Mat::zeros(m_input.size(), CV_32FC3);
    for (int i : inlierIdx)
        m_inliers.at<cv::Vec3f>(pix[i]) = pts[i];

    spdlog::trace("[RANSAC] refit on {} inliers, center=({:.3f}, {:.3f}, {:.3f})",
        inlierIdx.size(), plane.origin.x, plane.origin.y, plane.origin.z);
    return plane;
}
