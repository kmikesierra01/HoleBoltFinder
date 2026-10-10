#include "pch.h"
#include "HoleFinder.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <filesystem>

// -- execute ---------------------------------------------------------------------

std::optional<Circle3D> HoleFinder::execute() const
{
    spdlog::info("[Hole] Starting hole detection.");

    // Ensure debug directory exists
    std::filesystem::create_directories("debug");

    cv::Mat binary;
    if (!toMask(m_pc, binary))
    {
        spdlog::error("[Hole] Failed to binarize point cloud.");
        return std::nullopt;
    }

    cv::imwrite("debug/01_binary_before_invert.png", binary);
    spdlog::trace("[Hole] Saved debug/01_binary_before_invert.png ({}x{})", binary.cols, binary.rows);

    // Invert: hole pixels become white
    cv::bitwise_not(binary, binary);
    cv::imwrite("debug/02_binary_inverted.png", binary);

    if (binary.rows <= 10 || binary.cols <= 10)
    {
        spdlog::error("[Hole] Image too small: {}x{}", binary.rows, binary.cols);
        return std::nullopt;
    }

    // Flood fill from all 4 borders to remove border-connected voids.
    // 1px padding added so floodFill can reach corner-touching white regions.
    {
        cv::Mat padded;
        cv::copyMakeBorder(binary, padded, 1, 1, 1, 1, cv::BORDER_CONSTANT, cv::Scalar(255));
        cv::floodFill(padded, cv::Point(0, 0), cv::Scalar(0));
        binary = padded(cv::Rect(1, 1, binary.cols, binary.rows)).clone();
    }
    cv::imwrite("debug/02b_flood_fill.png", binary);
    spdlog::trace("[Hole] Saved debug/02b_flood_fill.png");

    cv::Mat filtered;
    if (!openClose(binary, 3, filtered))
        filtered = binary;

    cv::imwrite("debug/03_morphology.png", filtered);
    spdlog::trace("[Hole] Saved debug/03_morphology.png");

    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(filtered, contours, hierarchy, cv::RETR_TREE, cv::CHAIN_APPROX_NONE);

    spdlog::trace("[Hole] Contour count: {}", contours.size());

    // Draw all contours with hierarchy info for debug
    if (!contours.empty())
    {
        cv::Mat dbgContours;
        cv::cvtColor(filtered, dbgContours, cv::COLOR_GRAY2BGR);
        for (int i = 0; i < static_cast<int>(contours.size()); ++i)
        {
            int parent = hierarchy[i][3];
            int grandparent = (parent >= 0) ? hierarchy[parent][3] : -1;
            cv::Scalar color = (parent >= 0 && grandparent >= 0)
                ? cv::Scalar(0, 255, 0)    // inner hole candidate (green)
                : cv::Scalar(0, 0, 255);   // outer contour (red)
            cv::drawContours(dbgContours, contours, i, color, 1);
        }
        cv::imwrite("debug/04_contours.png", dbgContours);
        spdlog::trace("[Hole] Saved debug/04_contours.png ({} contours)", contours.size());

        // Log hierarchy for each contour
        for (int i = 0; i < static_cast<int>(contours.size()); ++i)
        {
            int parent = hierarchy[i][3];
            int grandparent = (parent >= 0) ? hierarchy[parent][3] : -1;
            double area = cv::contourArea(contours[i]);
            spdlog::trace("[Hole] Contour[{}] area={:.1f} parent={} grandparent={}", i, area, parent, grandparent);
        }
    }

    if (contours.empty())
    {
        spdlog::error("[Hole] No contours found.");
        return std::nullopt;
    }

    std::vector<cv::Point> holeContour;
    if (!findLargestInnerHole(contours, holeContour))
    {
        spdlog::error("[Hole] No inner hole found.");
        return std::nullopt;
    }

    std::vector<cv::Point3f> pts3d;
    for (const auto& p2d : holeContour)
    {
        cv::Point3f p3d;
        if (pointAt(m_pc, p2d, 1, p3d))
            pts3d.push_back(p3d);
    }

    auto result = fitCircle(pts3d);
    if (!result)
    {
        spdlog::error("[Hole] Circle fitting failed.");
        return std::nullopt;
    }

    result->contourPoints = pts3d;

    // Build result image: contour (green) + fitted circle (cyan) on binary
    {
        cv::Mat img;
        cv::cvtColor(filtered, img, cv::COLOR_GRAY2BGR);
        cv::drawContours(img, std::vector<std::vector<cv::Point>>{holeContour}, 0, cv::Scalar(0, 220, 0), 2);
        cv::Point2f c2d(0.0f, 0.0f);
        for (const auto& p : holeContour) { c2d.x += static_cast<float>(p.x); c2d.y += static_cast<float>(p.y); }
        c2d *= (1.0f / static_cast<float>(holeContour.size()));
        float r2d = 0.0f;
        for (const auto& p : holeContour) {
            float dx = static_cast<float>(p.x) - c2d.x, dy = static_cast<float>(p.y) - c2d.y;
            r2d += std::sqrt(dx * dx + dy * dy);
        }
        r2d /= static_cast<float>(holeContour.size());
        cv::circle(img, c2d, static_cast<int>(r2d + 0.5f), cv::Scalar(0, 200, 255), 2);
        cv::circle(img, c2d, 4, cv::Scalar(0, 200, 255), -1);
        m_resultImage = img;
    }

    // (result logged by MainController after planeCenter/planeNormal set)
    return result;
}

// -- fitCircle (Kasa least squares) ------------------------------------------

std::optional<Circle3D> HoleFinder::fitCircle(const std::vector<cv::Point3f>& pts)
{
    if (pts.size() < 3) return std::nullopt;

    // Centroid
    cv::Point3f centroid(0, 0, 0);
    for (const auto& p : pts) centroid += p;
    centroid *= (1.0f / static_cast<float>(pts.size()));

    // PCA -> plane normal + local axes
    const int n = static_cast<int>(pts.size());
    cv::Mat data(n, 3, CV_32F);
    for (int i = 0; i < n; ++i)
    {
        data.at<float>(i, 0) = pts[i].x - centroid.x;
        data.at<float>(i, 1) = pts[i].y - centroid.y;
        data.at<float>(i, 2) = pts[i].z - centroid.z;
    }
    cv::PCA pca(data, cv::Mat(), cv::PCA::DATA_AS_ROW);
    cv::Vec3f u = pca.eigenvectors.row(0);  // max variance
    cv::Vec3f v = pca.eigenvectors.row(1);  // 2nd variance
    cv::Vec3f normal = pca.eigenvectors.row(2);  // normal

    // Project to 2D and solve Kasa: Dx + Ey + F = -(x^2+y^2)
    cv::Mat A(n, 3, CV_64F), b(n, 1, CV_64F);
    for (int i = 0; i < n; ++i)
    {
        cv::Vec3f dp(pts[i].x - centroid.x, pts[i].y - centroid.y, pts[i].z - centroid.z);
        double px = dp.dot(u);
        double py = dp.dot(v);
        A.at<double>(i, 0) = px;
        A.at<double>(i, 1) = py;
        A.at<double>(i, 2) = 1.0;
        b.at<double>(i, 0) = -(px * px + py * py);
    }

    cv::Mat coeff;
    if (!cv::solve(A, b, coeff, cv::DECOMP_SVD)) return std::nullopt;

    double D = coeff.at<double>(0);
    double E = coeff.at<double>(1);
    double F = coeff.at<double>(2);
    double cx = -D / 2.0, cy = -E / 2.0;
    double r   = std::sqrt(cx * cx + cy * cy - F);

    if (r <= 0.0 || !std::isfinite(r)) return std::nullopt;

    cv::Point3f center(
        centroid.x + static_cast<float>(cx) * u[0] + static_cast<float>(cy) * v[0],
        centroid.y + static_cast<float>(cx) * u[1] + static_cast<float>(cy) * v[1],
        centroid.z + static_cast<float>(cx) * u[2] + static_cast<float>(cy) * v[2]);

    return Circle3D{ center, normal, static_cast<float>(r) };
}

// -- Private helpers ------------------------------------------------------------

bool HoleFinder::openClose(const cv::Mat& src, int kernelSize, cv::Mat& dst) const
{
    if (src.empty() || kernelSize < 1) return false;
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(kernelSize, kernelSize));
    cv::Mat tmp;
    cv::morphologyEx(src, tmp, cv::MORPH_OPEN,  kernel, cv::Point(-1, -1), 1);
    cv::morphologyEx(tmp, dst, cv::MORPH_CLOSE, kernel, cv::Point(-1, -1), 1);
    return true;
}

bool HoleFinder::toMask(const cv::Mat& pc, cv::Mat& dst) const
{
    if (pc.empty() || pc.type() != CV_32FC3) return false;
    dst = cv::Mat::zeros(pc.size(), CV_8UC1);
    for (int r = 0; r < pc.rows; ++r)
        for (int c = 0; c < pc.cols; ++c)
            if (isValidPoint(pc.at<cv::Vec3f>(r, c)))
                dst.at<uchar>(r, c) = 255;
    return true;
}

bool HoleFinder::findLargestInnerHole(const std::vector<std::vector<cv::Point>>& contours,
                                       std::vector<cv::Point>& out) const
{
    int bestIdx = -1;
    float maxArea = 0.0f;
    for (int i = 0; i < static_cast<int>(contours.size()); ++i)
    {
        float area = static_cast<float>(cv::contourArea(contours[i]));
        if (area > maxArea) { maxArea = area; bestIdx = i; }
    }
    if (bestIdx < 0) return false;
    out = contours[bestIdx];
    return true;
}

bool HoleFinder::pointAt(const cv::Mat& pc, cv::Point px, int range, cv::Point3f& out) const
{
    const int r = px.y, c = px.x;
    if (r < 0 || r >= pc.rows || c < 0 || c >= pc.cols) return false;

    cv::Vec3f v = pc.at<cv::Vec3f>(r, c);
    if (isValidPoint(v)) { out = cv::Point3f(v); return true; }

    // 8-direction search within range
    cv::Vec3f sum(0, 0, 0);
    int count = 0;
    bool found[8] = {};
    const int dr[] = {-1,1,0,0,-1,-1,1,1};
    const int dc[] = {0,0,-1,1,-1,1,-1,1};

    for (int s = 1; s <= range; ++s)
        for (int d = 0; d < 8; ++d)
        {
            if (found[d]) continue;
            int nr = r + dr[d]*s, nc = c + dc[d]*s;
            if (nr < 0 || nr >= pc.rows || nc < 0 || nc >= pc.cols) continue;
            cv::Vec3f nb = pc.at<cv::Vec3f>(nr, nc);
            if (isValidPoint(nb)) { sum += nb; ++count; found[d] = true; }
        }

    if (count == 0) return false;
    out = cv::Point3f(sum / static_cast<float>(count));
    return true;
}
