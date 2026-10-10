#pragma once

#include <opencv2/core.hpp>
#include <cmath>

// -- Utility ---------------------------------------------------
inline bool isValidPoint(const cv::Vec3f& v)
{
    return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2])
        && (v[0] != 0.0f || v[1] != 0.0f || v[2] != 0.0f);
}
inline bool isValidPoint(const cv::Point3f& p) { return isValidPoint(cv::Vec3f(p.x, p.y, p.z)); }

// -- Point Cloud -----------------------------------------------
struct PCData
{
    cv::Mat matPointCloud;  // CV_32FC3 (XYZ), height x width, NaN = invalid
    float   maxZ;
    float   minZ;
    int     pointCount;
};

// -- Plane: origin + tips of two in-plane unit axes -------------
struct Plane
{
    cv::Point3f origin;
    cv::Point3f xEnd;  // origin + unit X axis
    cv::Point3f yEnd;  // origin + unit Y axis

    bool empty() const { return origin == cv::Point3f() && xEnd == cv::Point3f(); }
};

inline cv::Vec3f planeNormal(const Plane& p)
{
    return cv::normalize(cv::Vec3f(p.xEnd - p.origin).cross(cv::Vec3f(p.yEnd - p.origin)));
}

// -- Circle in 3D ---------------------------------------------
struct Circle3D
{
    cv::Point3f center;
    cv::Vec3f   normal;       // circle fitting normal (PCA of contour points)
    float       radius = 0.0f;
    float       height = 0.0f;               // protrusion height from reference plane (bolt only)
    cv::Point3f planeCenter;                 // centroid of reference plane (RANSAC)
    cv::Vec3f   planeNormal;                 // normal of reference plane (RANSAC)
    std::vector<cv::Point3f> contourPoints;  // 3D contour points used for fitting
};

// -- Cropped region (pointcloud + texture) --------------------
struct CroppedData
{
    cv::Mat pointCloud;  // CV_32FC3
    cv::Mat texture;     // CV_8UC1 or CV_8UC3 (may be empty)
};

// -- Algorithm Params -----------------------------------------
struct HoleRansacParams
{
    double distanceThreshold = 0.3;
};

struct BoltSearchParams
{
    float floorRemovalDist = 1.0f;
    float surfaceLayer     = 2.0f;
};

struct AlgoParams
{
    // TODO: algorithm tuning parameters
};

// -- Inspection Result ----------------------------------------
struct InspectionResult
{
    // TODO: hole center, bolt height, etc.
};
