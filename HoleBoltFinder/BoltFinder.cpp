#include "pch.h"
#include "BoltFinder.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/flann.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>

// -- execute --------------------------------------------------

std::optional<Circle3D> BoltFinder::execute() const
{

    std::filesystem::create_directories("debug");

    // 1. Find reference plane via RANSAC (max 500 iterations)
    // minInliers is set to 20% of valid points so it works regardless of crop size
    PlaneRansac ransac;
    ransac.setInput(m_pc);
    ransac.setIterations(500);
    {
        int validCount = 0;
        for (int r = 0; r < m_pc.rows; ++r)
            for (int c = 0; c < m_pc.cols; ++c)
                if (isValidPoint(m_pc.at<cv::Vec3f>(r, c))) ++validCount;
        const int minInlier = std::max(10, static_cast<int>(validCount * 0.2f));
        ransac.setInlierDistance(1.5);
        ransac.setMinInliers(minInlier);
        spdlog::info("[Bolt] RANSAC minInliers={} (20% of {} valid pts), PlaneInlierDist=1.5mm", minInlier, validCount);
    }
    auto planeOpt = ransac.execute();
    if (!planeOpt)
    {
        spdlog::error("[Bolt] Failed to find reference plane via RANSAC.");
        return std::nullopt;
    }
    m_debugPlane = *planeOpt;  // Store for debug inspection

    // 2. Remove the floor; keep the dominant layer above it (height histogram peak)
    cv::Mat filtered = keepTopLayer(m_pc, *planeOpt);
    cv::imwrite("debug/bolt_01_plane_filter.png", toMask(filtered));
    spdlog::trace("[Bolt] Saved debug/bolt_01_plane_filter.png");

    // 3. Statistical outlier removal (kNN-based)
    cv::Mat outlierFiltered = removeOutliers(filtered);
    cv::imwrite("debug/bolt_02_outlier_filter.png", toMask(outlierFiltered));
    spdlog::trace("[Bolt] Saved debug/bolt_02_outlier_filter.png");

    // 4. Binarize
    cv::Mat binary = toMask(outlierFiltered);
    cv::imwrite("debug/bolt_03_binary.png", binary);
    spdlog::trace("[Bolt] Saved debug/bolt_03_binary.png ({}x{})", binary.cols, binary.rows);
    if (binary.rows <= 10 || binary.cols <= 10)
    {
        spdlog::error("[Bolt] Image too small: {}x{}", binary.rows, binary.cols);
        return std::nullopt;
    }

    // 5. Morphology cleanup (kernel=1)
    cv::Mat clean = openClose(binary, 1);
    cv::imwrite("debug/bolt_04_morphology.png", clean);
    spdlog::trace("[Bolt] Saved debug/bolt_04_morphology.png");

    // 6. Find largest external contour
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(clean, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
    if (contours.empty())
    {
        spdlog::error("[Bolt] No contours found.");
        return std::nullopt;
    }

    // Sort by area descending, take largest
    std::sort(contours.begin(), contours.end(),
        [](const auto& a, const auto& b) { return cv::contourArea(a) > cv::contourArea(b); });

    // Draw contours debug image
    {
        cv::Mat dbg;
        cv::cvtColor(clean, dbg, cv::COLOR_GRAY2BGR);
        cv::drawContours(dbg, contours, 0, cv::Scalar(0, 255, 0), 1);
        cv::imwrite("debug/bolt_05_contours.png", dbg);
        spdlog::trace("[Bolt] Saved debug/bolt_05_contours.png ({} contours)", contours.size());
    }

    // Collect 3D points: first try contour boundary pixels
    std::vector<cv::Point3f> pts3d;
    for (const auto& p2d : contours.front())
    {
        cv::Point3f p3d;
        if (pointAt(outlierFiltered, p2d, 1, p3d))
            pts3d.push_back(p3d);
    }

    // Sparse scan fallback: if contour pixels didn't yield enough points,
    // collect ALL valid points from outlierFiltered (already height-filtered & outlier-removed)
    if (pts3d.size() < 4)
    {
        pts3d.clear();
        for (int r = 0; r < outlierFiltered.rows; ++r)
            for (int c = 0; c < outlierFiltered.cols; ++c)
            {
                const auto& v = outlierFiltered.at<cv::Vec3f>(r, c);
                if (isValidPoint(v))
                    pts3d.push_back(cv::Point3f(v));
            }
        spdlog::warn("[Bolt] Sparse contour, using all {} valid pts from filtered cloud", pts3d.size());
    }

    spdlog::info("[Bolt] Largest contour: {} px, valid 3D pts: {}", contours.front().size(), pts3d.size());

    auto result = HoleFinder::fitCircle(pts3d);
    if (!result)
    {
        spdlog::error("[Bolt] Circle fitting failed. pts3d={} (need >=4, non-collinear)", pts3d.size());
        return std::nullopt;
    }

    // Populate contour points for visualization
    result->contourPoints = pts3d;

    // Build result image: largest contour (green) + fitted circle (cyan)
    {
        cv::Mat img;
        cv::cvtColor(clean, img, cv::COLOR_GRAY2BGR);
        cv::drawContours(img, contours, 0, cv::Scalar(0, 220, 0), 2);
        const auto& bc = contours.front();
        cv::Point2f c2d(0.0f, 0.0f);
        for (const auto& p : bc) { c2d.x += static_cast<float>(p.x); c2d.y += static_cast<float>(p.y); }
        c2d *= (1.0f / static_cast<float>(bc.size()));
        float r2d = 0.0f;
        for (const auto& p : bc) {
            float dx = static_cast<float>(p.x) - c2d.x, dy = static_cast<float>(p.y) - c2d.y;
            r2d += std::sqrt(dx * dx + dy * dy);
        }
        r2d /= static_cast<float>(bc.size());
        cv::circle(img, c2d, static_cast<int>(r2d + 0.5f), cv::Scalar(0, 200, 255), 2);
        cv::circle(img, c2d, 4, cv::Scalar(0, 200, 255), -1);
        m_resultImage = img;
    }

    // Compute protrusion height: perpendicular distance from RANSAC plane to bolt center
    const cv::Vec3f normal = planeNormal(*planeOpt);
    cv::Vec3f origin(planeOpt->origin.x, planeOpt->origin.y, planeOpt->origin.z);
    cv::Vec3f toCenter(result->center.x - origin[0], result->center.y - origin[1], result->center.z - origin[2]);
    result->height = std::abs(toCenter.dot(normal));

    // (result logged by MainController after planeCenter/planeNormal set)
    return result;
}

// -- Outlier filter -------------------------------------------

cv::Mat BoltFinder::removeOutliers(const cv::Mat& pc) const
{
    cv::Mat result = pc;
    for (int i = 0; i < m_outlierIterations; ++i)
        result = statisticalOutlierPass(result, m_outlierKNeighbors, m_outlierStdDevMult);
    return result;
}

cv::Mat BoltFinder::statisticalOutlierPass(const cv::Mat& pc, int kNeighbors, float stdDevMult) const
{
    if (pc.empty() || pc.type() != CV_32FC3 || kNeighbors < 2 || stdDevMult <= 0.f)
        return pc;

    std::vector<cv::Point2i> pixelIdx;
    std::vector<cv::Vec3f>   validPts;
    pixelIdx.reserve(static_cast<size_t>(pc.total()));
    validPts.reserve(static_cast<size_t>(pc.total()));

    const float eps = 1e-6f;
    for (int r = 0; r < pc.rows; ++r)
    {
        const cv::Vec3f* row = pc.ptr<cv::Vec3f>(r);
        for (int c = 0; c < pc.cols; ++c)
        {
            const cv::Vec3f& v = row[c];
            if (std::abs(v[0]) > eps || std::abs(v[1]) > eps || std::abs(v[2]) > eps)
            {
                pixelIdx.emplace_back(c, r);
                validPts.emplace_back(v);
            }
        }
    }

    const int numPts = static_cast<int>(validPts.size());
    if (numPts <= kNeighbors)
        return pc;

    cv::Mat ptsMat(numPts, 3, CV_32F);
    for (int i = 0; i < numPts; ++i)
    {
        ptsMat.at<float>(i, 0) = validPts[i][0];
        ptsMat.at<float>(i, 1) = validPts[i][1];
        ptsMat.at<float>(i, 2) = validPts[i][2];
    }

    cv::flann::Index flannIdx(ptsMat, cv::flann::KDTreeIndexParams(1));
    const int knn = kNeighbors + 1;
    std::vector<int>    nbIndices(knn);
    std::vector<float>  nbDists(knn);
    std::vector<double> meanDists(numPts);

    for (int i = 0; i < numPts; ++i)
    {
        flannIdx.knnSearch(ptsMat.row(i), nbIndices, nbDists, knn, cv::flann::SearchParams(-1));
        std::sort(nbDists.begin(), nbDists.end());

        int start = (!nbIndices.empty() && nbIndices[0] == i) ? 1 : 0;
        double sum = 0.0;
        int cnt = 0;
        for (int n = start; n < knn; ++n) { sum += nbDists[n]; ++cnt; }
        meanDists[i] = cnt > 0 ? sum / cnt : 0.0;
    }

    double mean = 0.0, sqSum = 0.0;
    for (double d : meanDists) { mean += d; sqSum += d * d; }
    mean /= numPts;
    const double stddev = std::sqrt(std::max(0.0, sqSum / numPts - mean * mean));
    const double thresh = stdDevMult * stddev;

    cv::Mat out = cv::Mat::zeros(pc.size(), CV_32FC3);
    int kept = 0;
    for (int i = 0; i < numPts; ++i)
    {
        if (std::abs(meanDists[i] - mean) <= thresh)
        {
            out.at<cv::Vec3f>(pixelIdx[i].y, pixelIdx[i].x) = validPts[i];
            ++kept;
        }
    }

    spdlog::info("[Bolt] Outlier removal: {} -> {} points", numPts, kept);
    return out;
}

// -- Private helpers -------------------------------------------

cv::Mat BoltFinder::keepTopLayer(const cv::Mat& pc, const Plane& plane) const
{
    cv::Vec3f normal = planeNormal(plane);
    cv::Vec3f origin(plane.origin.x, plane.origin.y, plane.origin.z);

    // PCA eigenvector sign is arbitrary; orient normal toward the sensor (+Z side)
    if (normal[2] < 0.0f)
        normal = -normal;

    // Step 1: collect signed distances above the floor threshold
    std::vector<float> dists;
    dists.reserve(static_cast<size_t>(pc.rows) * pc.cols);
    for (int r = 0; r < pc.rows; ++r)
        for (int c = 0; c < pc.cols; ++c)
        {
            const auto& v = pc.at<cv::Vec3f>(r, c);
            if (!isValidPoint(v)) continue;
            float d = (v - origin).dot(normal);
            if (d > m_floorRemovalDist)
                dists.push_back(d);
        }

    spdlog::info("[Bolt] keepTopLayer: {} pts above floorRemovalDist={:.2f}mm (total valid in crop: {})",
        dists.size(), m_floorRemovalDist,
        [&]{ int n=0; for(int r=0;r<pc.rows;++r) for(int c=0;c<pc.cols;++c) if(isValidPoint(pc.at<cv::Vec3f>(r,c))) ++n; return n; }());

    if (dists.empty())
    {
        spdlog::warn("[Bolt] keepTopLayer: no points above floorRemovalDist={:.2f}mm", m_floorRemovalDist);
        return pc;
    }

    // Step 2: build height histogram (0.2mm bins)
    constexpr float binSize = 0.2f;
    const float maxDist = *std::max_element(dists.begin(), dists.end());
    const int numBins = std::max(1, static_cast<int>((maxDist - m_floorRemovalDist) / binSize) + 1);

    std::vector<int> hist(static_cast<size_t>(numBins), 0);
    for (float d : dists)
    {
        int bin = std::clamp(static_cast<int>((d - m_floorRemovalDist) / binSize), 0, numBins - 1);
        ++hist[static_cast<size_t>(bin)];
    }

    // Step 3: find the peak bin (dominant height layer = bolt/board surface)
    const int peakBin = static_cast<int>(std::max_element(hist.begin(), hist.end()) - hist.begin());
    const float peakDist = m_floorRemovalDist + (peakBin + 0.5f) * binSize;

    spdlog::info("[Bolt] Bolt surface detected at +{:.2f}mm above plane (floor removal: >+{:.2f}mm, retain +/-{:.2f}mm -> {}pts result)",
        peakDist, m_floorRemovalDist, m_surfaceLayer, hist[static_cast<size_t>(peakBin)]);

    // Step 4: keep only points within tolerance of the peak
    cv::Mat out = pc.clone();
    for (int r = 0; r < out.rows; ++r)
        for (int c = 0; c < out.cols; ++c)
        {
            auto& v = out.at<cv::Vec3f>(r, c);
            if (!isValidPoint(v)) continue;
            float d = (v - origin).dot(normal);
            if (std::abs(d - peakDist) > m_surfaceLayer)
                v = cv::Vec3f(0, 0, 0);
        }
    return out;
}

cv::Mat BoltFinder::toMask(const cv::Mat& pc) const
{
    cv::Mat dst = cv::Mat::zeros(pc.size(), CV_8UC1);
    for (int r = 0; r < pc.rows; ++r)
        for (int c = 0; c < pc.cols; ++c)
            if (isValidPoint(pc.at<cv::Vec3f>(r, c)))
                dst.at<uchar>(r, c) = 255;
    return dst;
}

cv::Mat BoltFinder::openClose(const cv::Mat& src, int kernelSize) const
{
    if (src.empty() || kernelSize < 1) return src;
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(kernelSize, kernelSize));
    cv::Mat tmp, dst;
    cv::morphologyEx(src, tmp, cv::MORPH_OPEN,  kernel, cv::Point(-1,-1), 1);
    cv::morphologyEx(tmp, dst, cv::MORPH_CLOSE, kernel, cv::Point(-1,-1), 1);
    return dst;
}

bool BoltFinder::pointAt(const cv::Mat& pc, cv::Point px, int range, cv::Point3f& out) const
{
    const int r = px.y, c = px.x;
    if (r < 0 || r >= pc.rows || c < 0 || c >= pc.cols) return false;

    cv::Vec3f v = pc.at<cv::Vec3f>(r, c);
    if (isValidPoint(v)) { out = cv::Point3f(v); return true; }

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
