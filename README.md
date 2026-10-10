# HoleBoltFinder

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform: Windows](https://img.shields.io/badge/Platform-Windows%2010%2F11-0078D6?logo=windows&logoColor=white)](#빌드-방법)
![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)
![Qt 5.15.2](https://img.shields.io/badge/Qt-5.15.2-41CD52?logo=qt&logoColor=white)
![OpenCV 4.8.0](https://img.shields.io/badge/OpenCV-4.8.0-5C3EE8?logo=opencv&logoColor=white)

## 🌐 [English version → README_EN.md](README_EN.md)

3D 포인트클라우드에서 **구멍(오목)** 또는 **볼트(돌출)** 를 자동으로 감지하여 원의 중심·반지름·법선을 반환하는 Qt + OpenGL 기반 데스크탑 도구.

![Demo](docs/images/ui_manipulation.gif)

---

## 다운로드

→ **[Releases 페이지](../../releases)** 에서 최신 버전을 받으세요.

압축 해제 후 `HoleBoltFinder.exe` 실행. 별도 설치 불필요.

---

## 빠른 시작 (Demo)

1. `HoleBoltFinder.exe` 와 같은 폴더에 `presets.json` 및 `samples/` 폴더가 있는지 확인
2. 프로그램 실행
3. 상단 콤보박스에서 데모 프리셋 선택
4. **Run Demo** 버튼을 눌러 포인트클라우드를 로드한 뒤, 다시 눌러 Crop 영역으로 카메라를 이동하고, 한 번 더 눌러 감지 실행

> 기존 `presets.ini` 파일은 최초 실행 시 `presets.json`으로 자동 변환되며, 원본은 `presets.ini.bak`으로 보존됩니다.

---

## 목차

**사용자**
- [개요](#개요)
- [화면 구성](#화면-구성)
- [사용 방법](#사용-방법)
- [파라미터 설명](#파라미터-설명)

**개발자**
- [빌드 방법](#빌드-방법)
- [알고리즘](#알고리즘)
- [디버그 이미지](#디버그-이미지)
- [presets.json 형식](#presetsjson-형식)

---

## 개요

| 항목 | 내용 |
|------|------|
| 입력 | `.pcd` (ascii / binary / binary_compressed) |
| 출력 | 감지된 원의 중심 좌표, 반지름, 법선 벡터 (3D) |
| 모드 | **HoleFinder** (오목 구멍) / **BoltFinder** (돌출 볼트) |
| 뷰어 | QOpenGLWidget 기반 3D 포인트클라우드 뷰어 |
| 설정 저장 | `Setting.ini` -> 파라미터 자동 저장·복원 |

---

## 화면 구성

![UI Screenshot](docs/images/screenshot_ui.jpg)

```
+-------------------------------------------------------------------+
|  Run Demo  | 1/2 |  [Preset 콤보박스]  | +Target | +Preset |     |
|            |     |                     |         |         |     |
|                   Hole Search    Bolt Search           시계     |
+--------------------------------+----------------------------------+
|                                | [ Source Open ]  [ Save Setting ]|
|  3D 포인트클라우드 뷰어        +----------------------------------+
|  (QOpenGLWidget)               | Detection Result                 |
|                                | Type         -                   |
|  - 가운데 버튼 드래그: 회전    | Center (mm)  -                   |
|    (쿼터니언 트랙볼)           | Radius (mm)  -                   |
|  - 우클릭 드래그: 이동         | Plane Center -                   |
|  - Ctrl+클릭: Crop 중심 설정   | Plane Normal -                   |
|  - Crop 박스 오버레이          +----------------------------------+
|  - 결과 컨투어 / 구체 표시     | Crop                             |
|                                | Size  X:    Y:    Z:             |
|                                | Pos   X:    Y:    Z:             |
|                                +----------------------------------+
|                                | Hole Finder                      |
|                                | Plane Fit Tolerance (mm)         |
|                                +----------------------------------+
|                                | Bolt Finder                      |
|                                | Floor Removal Dist. (mm)         |
|                                | Surface Layer (±mm)              |
|                                +----------------------------------+
|                                | 로그 (QPlainTextEdit)            |
+--------------------------------+----------------------------------+
| Last Action: -                              Point Count: -        |
+-------------------------------------------------------------------+
```

---

## 사용 방법

### 1. 파일 로드

`Source Open` 버튼 → `.pcd` 포인트클라우드 파일 선택.
뷰어에 전체 포인트클라우드가 표시됨.

> **지원 PCD 포맷:** ascii, binary, binary_compressed (LZF)

### 2. Crop 영역 설정

| 방법 | 설명 |
|------|------|
| Ctrl+클릭 | 클릭 지점을 Crop 중심으로 자동 설정 |
| 스핀박스 직접 입력 | Crop 섹션의 Pos X/Y/Z (중심), Size X/Y/Z (박스 크기) |

뷰어에 반투명 Crop 박스가 오버레이로 표시됨.

> **Tip**: Crop 박스는 대상(구멍/볼트)보다 충분히 크게 설정하는 것이 권장.
> 너무 좁으면 RANSAC 기준면이 바닥이 아닌 보드 표면으로 맞힐 수 있음.

### 3. 감지 실행

| 버튼 | 동작 |
|------|------|
| `Hole Search` | Crop → HoleFinder 실행 → 구멍 컨투어 시각화 |
| `Bolt Search` | Crop → BoltFinder 실행 → 볼트 컨투어 + 중심 마커 시각화 |

검색 버튼 클릭 시 자동으로 Crop 먼저 실행 후 감지 실행.
감지 실패 시 이전 결과 시각화도 자동으로 제거됨.

### 4. 결과 확인

우측 패널 **Detection Result** 박스에서 수치 확인:

| 항목 | 내용 |
|------|------|
| Type | Hole / Bolt |
| Center (mm) | 감지된 원의 3D 중심 좌표 |
| Radius (mm) | 반지름 |
| Plane Center | 기준 평면의 중심점 |
| Plane Normal | 기준 평면의 법선 벡터 |

뷰어에서도 확인 가능:
- **컨투어 (노란 선)**: 감지된 원의 윤곽선
- **빨강 원 마커**: 원의 중심점

![Detection Result](docs/images/screenshot_result_1.jpg)

![Detection Result Panel](docs/images/screenshot_result_2.jpg)

---

## 알고리즘

### HoleFinder

```
입력 포인트클라우드 (CV_32FC3)
        |
        v
   2D 바이너리 이미지 변환 (유효 포인트 -> 255)
        |
        v
   Morphology 노이즈 제거 (OPEN -> CLOSE)
        |
        v
   계층형 컨투어 트리 (RETR_TREE)
   -> 외곽 연결 빈 영역 제거 후 가장 큰 컨투어 선택
        |
        v
   2D 컨투어 픽셀 -> 3D 좌표 변환 (이웃 탐색 포함)
        |
        v
   Kasa 최소제곱 3D 원 피팅
        |
        v
   Circle3D (center, normal, radius)
```

### BoltFinder

```
입력 포인트클라우드 (CV_32FC3)
        |
        v
1. RANSAC 평면 감지 (500회 반복, dist_th=1.5mm, minInlier=20%, 인라이어 최소제곱 재적합)
        |
        v
2. 높이 필터링 (keepTopLayer)
   - RANSAC 기준면과의 거리 d 계산
   - d < floorRemovalDist -> 제거 (바닥 제거)
   - 0.2mm 간격 히스토그램 -> dominant peak 찾기
   - peak +/- tolerance 범위만 유지 (볼트 표면 레이어 추출)
        |
        v
3. Statistical Outlier Removal (kNN=20, stdDev=2.0, 2회 반복)
        |
        v
4. 2D 바이너리 이미지 변환
        |
        v
5. Morphology 노이즈 제거
        |
        v
6. 최대 외곽 컨투어 선택 -> 3D 좌표 추출
   (컨투어 수가 4개 미만이면 전체 유효 점으로 fallback)
        |
        v
7. Kasa 최소제곱 3D 원 피팅
        |
        v
   Circle3D (center, normal, radius, height)
```

### Kasa 최소제곱 원 피팅

PCA로 점군의 주평면을 구한 후 해당 평면에 투영, Kasa 방법으로 원 방정식 계수를 선형 최소제곱으로 풀어 원 중심·반지름을 구함.
최소 4개 이상의 비공면(non-collinear) 3D 점이 필요.

---

## 빌드 방법

> 이 섹션은 **개발자**를 위한 내용입니다. 프로그램을 다운로드하여 실행하려면 [Releases](../../releases) 페이지를 이용하세요.

### 빌드 환경

| 항목 | 버전 / 값 |
|------|----------|
| OS | Windows 10 / 11 x64 |
| IDE | Visual Studio 2022 |
| Compiler | MSVC v143 (C++17) |
| Build System | MSBuild `.vcxproj` |
| Qt | 5.15.2 msvc2019_64 |
| OpenCV | 4.8.0 (헤더·정적 라이브러리 저장소 내 포함) |
| spdlog | 1.x 헤더 전용 (저장소 내 포함) |

> OpenCV 헤더는 `include/opencv_480/`, import library는 `lib/opencv_480/`에 위치합니다.
> spdlog 헤더는 `include/spdlog/`에 위치합니다.

### 빌드 절차

1. Qt 5.15.2 (msvc2019_64) 설치 후 VS Qt 플러그인에 `5.15.2_msvc2019_64` 이름으로 경로 등록
2. `HoleBoltFinder.sln` 열기
3. 구성 `Release | x64` 선택
4. `Ctrl+Shift+B`

## 파라미터 설명

### Crop

| 파라미터 | 설명 |
|---------|------|
| Size X/Y/Z | Crop 박스 크기 (mm) |
| Pos X/Y/Z | Crop 박스 중심 좌표 (mm) |

### Bolt Finder

| 파라미터 | 기본값 | 설명 |
|---------|--------|------|
| Floor Removal Dist. (mm) | 1.0 | RANSAC 기준면으로부터 이 거리 미만의 점 제거 (바닥 노이즈 제거) |
| Surface Layer (±mm) | 2.0 | 높이 히스토그램 dominant peak 기준 ±이 범위만 유지 (볼트 표면 레이어 추출) |

> **Floor Removal Dist.**: 값이 너무 크면 볼트 하단이 잘릴 수 있음.
> **Surface Layer**: 값이 너무 작으면 점 부족으로 원 피팅 실패, 너무 크면 바닥 점 포함됨.

### Hole Finder

| 파라미터 | 기본값 | 설명 |
|---------|--------|------|
| Plane Fit Tolerance (mm) | 0.3 | RANSAC 평면 피팅 인라이어 판정 거리. 기준면에서 이 거리 이내의 점을 인라이어로 판정. |

> **Plane Fit Tolerance**: 값이 너무 크면 기울어진 면도 동일 평면으로 처리되어 정확도 저하. 너무 작으면 인라이어 부족으로 RANSAC 실패 가능.
> Demo 실행 시 프리셋의 `planeFitTolerance` 값이 이 파라미터로 자동 적용됨.
---

## presets.json 형식

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

> Hole 타입은 `planeFitTolerance`가 RANSAC 평면 거리 임계값으로 사용됩니다.
> Bolt 타입은 `floorRemovalDist`가 바닥 제거 거리, `surfaceLayer`가 dominant surface layer 주변의 높이 범위로 사용됩니다.

---

## 디버그 이미지

감지 실행 시 `debug/` 폴더에 단계별 이미지가 자동 저장됨.

| 파일 | 내용 |
|------|------|
| **BoltFinder** | |
| `bolt_01_plane_filter.png` | RANSAC + 높이 필터 후 |
| `bolt_02_outlier_filter.png` | Statistical Outlier Removal 후 |
| `bolt_03_binary.png` | 2D 바이너리 이미지 |
| `bolt_04_morphology.png` | Morphology 처리 후 |
| `bolt_05_contours.png` | 컨투어 감지 결과 (초록=최대, 빨강=나머지) |
| **HoleFinder** | |
| `01_binary_before_invert.png` | 2D 바이너리 이미지 (반전 전) |
| `02_binary_inverted.png` | 반전 후 (구멍 영역=흰색) |
| `02b_flood_fill.png` | 외곽 연결 흰 영역 플러드필 제거 후 |
| `03_morphology.png` | Morphology 처리 후 |
| `04_contours.png` | 컨투어 결과 (초록=내부 구멍 후보, 빨강=외곽) |

## 라이선스

MIT © [kmikesierra01](https://github.com/kmikesierra01)

UI 스타일시트(`theme.qss`)는 [QDarkStyleSheet](https://github.com/ColinDuquesnoy/QDarkStyleSheet)를 기반으로 합니다 — MIT © 2013-2019 Colin Duquesnoy.
