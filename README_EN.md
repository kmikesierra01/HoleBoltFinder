# HoleBoltFinder

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform: Windows](https://img.shields.io/badge/Platform-Windows%2010%2F11-0078D6?logo=windows&logoColor=white)](#build-instructions)
![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)
![Qt 5.15.2](https://img.shields.io/badge/Qt-5.15.2-41CD52?logo=qt&logoColor=white)
![OpenCV 4.8.0](https://img.shields.io/badge/OpenCV-4.8.0-5C3EE8?logo=opencv&logoColor=white)

## 🌐 [한국어 버전 → README.md](README.md)

A Qt + OpenGL desktop tool that automatically detects **holes (concave)** or **bolts (convex)** in 3D point clouds, returning the center, radius, and normal of the detected circle.

![Demo](docs/images/ui_manipulation.gif)

---

## Download

→ Get the latest release from the **[Releases page](../../releases)**.

Extract the zip and run `HoleBoltFinder.exe`. No installation required.

---

## Quick Start (Demo)

1. Ensure `presets.json` and the `samples/` folder are in the same directory as `HoleBoltFinder.exe`
2. Launch the application
3. Select a demo preset from the top combo box
4. Click **Run Demo** to load the point cloud, click again to move the camera to the crop region, then click once more to run detection

> Legacy `presets.ini` files are migrated to `presets.json` automatically on first launch and preserved as `presets.ini.bak`.

---

## Table of Contents

**Users**
- [Overview](#overview)
- [UI Layout](#ui-layout)
- [How to Use](#how-to-use)
- [Parameter Reference](#parameter-reference)

**Developers**
- [Build Instructions](#build-instructions)
- [Algorithm](#algorithm)
- [Debug Images](#debug-images)
- [presets.json Format](#presetsjson-format)

---

## Overview

| Item | Description |
|------|-------------|
| Input | `.pcd` (ascii / binary / binary_compressed) |
| Output | Detected circle center coordinates, radius, and normal vector (3D) |
| Mode | **HoleFinder** (concave holes) / **BoltFinder** (convex bolts) |
| Viewer | QOpenGLWidget-based 3D point cloud viewer |
| Settings | `Setting.ini` -- parameters are saved and restored automatically |

---

## UI Layout

![UI Screenshot](docs/images/screenshot_ui.jpg)

```
+-------------------------------------------------------------------+
|  Run Demo  | 1/2 |  [Preset combo box]  | +Target | +Preset |    |
|            |     |                      |         |         |    |
|                   Hole Search    Bolt Search               Clock  |
+--------------------------------+----------------------------------+
|                                | [ Source Open ] [ Save Setting ] |
|  3D Point Cloud Viewer         +----------------------------------+
|  (QOpenGLWidget)               | Detection Result                 |
|                                | Type         -                   |
|  - Middle button drag: Rotate  | Center (mm)  -                   |
|    (quaternion trackball)      | Radius (mm)  -                   |
|  - Right button drag: Pan      | Plane Center -                   |
|  - Ctrl+Click: Set crop center | Plane Normal -                   |
|                                +----------------------------------+
|  - Crop box overlay            | Crop                             |
|  - Result contour / sphere     | Size  X:    Y:    Z:             |
|                                | Pos   X:    Y:    Z:             |
|                                +----------------------------------+
|                                | Hole Finder                      |
|                                | Plane Fit Tolerance (mm)         |
|                                +----------------------------------+
|                                | Bolt Finder                      |
|                                | Floor Removal Dist. (mm)         |
|                                | Surface Layer (±mm)              |
|                                +----------------------------------+
|                                | Log (QPlainTextEdit)             |
+--------------------------------+----------------------------------+
| Last Action: -                              Point Count: -        |
+-------------------------------------------------------------------+
```
---

## How to Use

### 1. Load File

Click `Source Open` to select a `.pcd` point cloud file.
The full point cloud is displayed in the viewer.

> **Supported PCD formats:** ascii, binary, binary_compressed (LZF)

### 2. Set Crop Region

| Method | Description |
|--------|-------------|
| Ctrl+Click in viewer | The clicked point is automatically set as the crop center |
| Spinbox input | Size X/Y/Z (box dimensions), Pos X/Y/Z (center coordinates) in the Crop section |

A semi-transparent crop box overlay is shown in the viewer.

> **Tip**: Set the crop box large enough to contain the target (hole/bolt).
> If too small, RANSAC may fit the reference plane to the board surface instead of the floor.

### 3. Run Detection

| Button | Action |
|--------|--------|
| `Hole Search` | Crop → run HoleFinder → visualize hole contour |
| `Bolt Search` | Crop → run BoltFinder → visualize bolt contour + center marker |

Clicking a search button automatically crops the cloud first, then runs detection.
If detection fails, any previous visualization is cleared.

### 4. View Results

Check the **Detection Result** panel on the right:

| Field | Content |
|-------|---------|
| Type | Hole / Bolt |
| Center (mm) | 3D center coordinates of the detected circle |
| Radius (mm) | Radius |
| Plane Center | Center point of the reference plane |
| Plane Normal | Normal vector of the reference plane |

Also visible in the 3D viewer:
- **Contour (yellow line)**: Outline of the detected circle
- **Red circle marker**: Center point of the circle

![Detection Result](docs/images/screenshot_result_1.jpg)

![Detection Result Panel](docs/images/screenshot_result_2.jpg)

---
## Algorithm

### HoleFinder

```
Input point cloud (CV_32FC3)
        |
        v
   Convert to 2D binary image (valid points -> 255)
        |
        v
   Morphology noise removal (OPEN -> CLOSE)
        |
        v
   Hierarchical contour tree (RETR_TREE)
   -> Select the largest contour after border-connected void removal
        |
        v
   Map 2D contour pixels -> 3D coordinates (with neighbor search)
        |
        v
   Kasa least-squares 3D circle fitting
        |
        v
   Circle3D (center, normal, radius)
```

### BoltFinder

```
Input point cloud (CV_32FC3)
        |
        v
1. RANSAC plane detection (500 iterations, dist_th=1.5mm, minInlier=20%, least-squares refit on inliers)
        |
        v
2. Height filtering (keepTopLayer)
   - Compute distance d from RANSAC reference plane
   - d < floorRemovalDist -> remove (floor removal)
   - Build histogram with 0.2mm bins -> find dominant peak
   - Keep only peak +/- tolerance range (extract bolt surface layer)
        |
        v
3. Statistical Outlier Removal (kNN=20, stdDev=2.0, 2 iterations)
        |
        v
4. Convert to 2D binary image
        |
        v
5. Morphology noise removal
        |
        v
6. Select largest outer contour -> extract 3D coordinates
   (fallback to all valid points if fewer than 4 contour points)
        |
        v
7. Kasa least-squares 3D circle fitting
        |
        v
   Circle3D (center, normal, radius, height)
```

### Kasa Least-Squares Circle Fitting

PCA is used to find the principal plane of the point set, then the points are projected onto that plane. The Kasa method solves the circle equation coefficients via linear least squares to obtain the center and radius.
Requires at least 4 non-collinear 3D points.

---

## Build Instructions

> This section is for **developers**. To download and run the program, visit the [Releases](../../releases) page.

### Build Environment

| Item | Version / Value |
|------|----------------|
| OS | Windows 10 / 11 x64 |
| IDE | Visual Studio 2022 |
| Compiler | MSVC v143 (C++17) |
| Build System | MSBuild `.vcxproj` |
| Qt | 5.15.2 msvc2019_64 |
| OpenCV | 4.8.0 (headers and import library included in repo) |
| spdlog | 1.x header-only (included in repo) |

> OpenCV headers are located at `include/opencv_480/`, import library at `lib/opencv_480/`.
> spdlog headers are located at `include/spdlog/`.

### Build Steps

1. Install Qt 5.15.2 (msvc2019_64) and register as `5.15.2_msvc2019_64` in the VS Qt extension
2. Open `HoleBoltFinder.sln`
3. Select `Release | x64`
4. Press `Ctrl+Shift+B`
---
## Parameter Reference

### Crop

| Parameter | Description |
|-----------|-------------|
| Size X/Y/Z | Crop box dimensions (mm) |
| Pos X/Y/Z | Crop box center coordinates (mm) |

### Bolt Finder

| Parameter | Default | Description |
|-----------|---------|-------------|
| Floor Removal Dist. (mm) | 1.0 | Points closer than this distance to the RANSAC plane are removed (floor noise removal). |
| Surface Layer (±mm) | 2.0 | Only points within ±this range of the height histogram dominant peak are kept (bolt surface layer extraction). |

> **Floor Removal Dist.**: If too large, the bottom of the bolt may be clipped.
> **Surface Layer**: If too small, insufficient points remain for circle fitting; if too large, floor points may be included.

### Hole Finder

| Parameter | Default | Description |
|-----------|---------|-------------|
| Plane Fit Tolerance (mm) | 0.3 | RANSAC plane fitting inlier threshold. Points within this distance from the fitted plane are treated as inliers. |

> **Plane Fit Tolerance**: If too large, tilted surfaces may be grouped onto the same plane, reducing accuracy. If too small, RANSAC may fail due to insufficient inliers.
> When running a demo, the preset's `planeFitTolerance` value is automatically applied as this parameter.

## presets.json Format

```json
[
  {
    "name": "Preset_01",
    "points": [
      {
        "file": "samples/sample_bolt.pcd",
        "type": "Bolt",
        "cx": 929.9, "cy": 476.1, "cz": -218.2,
        "sx": 150, "sy": 150, "sz": 80,
        "floorRemovalDist": 3.0,
        "surfaceLayer": 2.0
      },
      {
        "file": "samples/sample_hole.pcd",
        "type": "Hole",
        "cx": 309.4, "cy": 380.8, "cz": 89.7,
        "sx": 43, "sy": 45, "sz": 80,
        "planeFitTolerance": 0.8
      }
    ]
  }
]
```

> For `Hole` entries, `planeFitTolerance` is the RANSAC plane distance threshold.
> For `Bolt` entries, `floorRemovalDist` is the floor removal distance and `surfaceLayer` is the height band around the dominant surface layer.

---

## Debug Images

Debug images are automatically saved to the `debug/` folder at each processing step during detection.

| File | Contents |
|------|----------|
| **BoltFinder** | |
| `bolt_01_plane_filter.png` | After RANSAC + height filtering |
| `bolt_02_outlier_filter.png` | After Statistical Outlier Removal |
| `bolt_03_binary.png` | 2D binary image |
| `bolt_04_morphology.png` | After morphology processing |
| `bolt_05_contours.png` | Contour detection result (green=largest, red=others) |
| **HoleFinder** | |
| `01_binary_before_invert.png` | 2D binary image (before inversion) |
| `02_binary_inverted.png` | After inversion (hole area = white) |
| `02b_flood_fill.png` | After flood-fill removal of border-connected white regions |
| `03_morphology.png` | After morphology processing |
| `04_contours.png` | Contour result (green=inner hole candidates, red=outer) |

## License

MIT © [kmikesierra01](https://github.com/kmikesierra01)

UI stylesheet (`theme.qss`) is based on [QDarkStyleSheet](https://github.com/ColinDuquesnoy/QDarkStyleSheet) — MIT © 2013-2019 Colin Duquesnoy.
