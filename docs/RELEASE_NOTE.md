## HoleBoltFinder v1.3.0

### Changes

- RANSAC plane finder rewritten
  - The plane is refitted on its inliers twice, so results vary much less between runs
  - Uses a fixed random seed, so the same input always gives the same result
- Point cloud input is now `.pcd` only (Open button and drag & drop)

### Notes

- No installation required — extract zip and run `HoleBoltFinder.exe`

### Requirements

- Windows 10 x64 or later

---

## HoleBoltFinder v1.3.0

### 변경사항

- RANSAC 평면 탐색 재작성
  - 찾은 평면을 인라이어로 두 번 재적합 — 실행마다 결과 편차가 크게 줄어듦
  - 고정 난수 시드 사용 — 같은 입력이면 항상 같은 결과
- 포인트클라우드 입력은 `.pcd`만 지원 (Open 버튼, 드래그 앤 드롭 모두)

### 참고

- 별도 설치 불필요 — 압축 해제 후 `HoleBoltFinder.exe` 실행

### 시스템 요구사항

- Windows 10 64비트 이상

---

## HoleBoltFinder v1.2.1

### Improvements

- Point cloud files can now be loaded by dragging them from Windows Explorer and dropping them onto the window
  - Works anywhere on the window, including the 3D and 2D view areas
  - Accepts the same formats as the Open button
  - When several files are dropped at once, the first supported one is loaded

### Notes

- No installation required — extract zip and run `HoleBoltFinder.exe`

### Requirements

- Windows 10 x64 or later

---

## HoleBoltFinder v1.2.1

### 개선사항

- 윈도우 탐색기에서 포인트클라우드 파일을 창으로 끌어다 놓으면 바로 로드됩니다
  - 3D / 2D 뷰를 포함해 창 어느 위치에 놓아도 동작
  - Open 버튼과 동일한 형식 지원
  - 여러 파일을 한 번에 놓으면 지원 형식 중 첫 번째 파일을 로드

### 참고

- 별도 설치 불필요 — 압축 해제 후 `HoleBoltFinder.exe` 실행

### 시스템 요구사항

- Windows 10 64비트 이상

---

## HoleBoltFinder v1.2.0

### Improvements

- Preset storage format changed from INI to JSON (`presets.json`)
  - Existing `presets.ini` is automatically migrated and renamed to `presets.ini.bak` on first launch
  - Points can now be reordered freely by editing the JSON array in any text editor — no renumbering required

### Bug Fixes

- Fixed a long-standing bug where registering a Hole target incorrectly saved Bolt parameters (`Floor Removal Dist`, `Surface Layer`) instead of the Hole-specific `Plane Fit Tolerance`
- JSON field names now match UI labels exactly: `planeFitTolerance` (Hole), `floorRemovalDist` / `surfaceLayer` (Bolt)
- Demo progress label now updates immediately after registering a new target
- Run Demo search step now logs the actual parameters used (e.g. `Plane Fit Tolerance: 0.700mm`)

### Notes

- No installation required — extract zip and run `HoleBoltFinder.exe`
- `presets.ini` from previous versions is migrated automatically on first launch

### Requirements

- Windows 10 x64 or later

---

## HoleBoltFinder v1.2.0

### 개선사항

- 프리셋 저장 형식을 INI에서 JSON으로 변경 (`presets.json`)
  - 기존 `presets.ini`는 최초 실행 시 자동으로 마이그레이션되며 `presets.ini.bak`으로 이름 변경
  - JSON 배열 구조이므로 텍스트 에디터에서 항목을 잘라 붙이는 것만으로 순서 변경 가능 — 번호 재지정 불필요

### 버그 수정

- Hole 타겟 등록 시 Hole 전용 파라미터(`Plane Fit Tolerance`) 대신 Bolt 파라미터(`Floor Removal Dist`, `Surface Layer`)가 잘못 저장되던 오래된 버그 수정
- JSON 필드명이 UI 레이블과 정확히 일치하도록 수정: `planeFitTolerance` (Hole), `floorRemovalDist` / `surfaceLayer` (Bolt)
- 타겟 등록 직후 Demo 진행 레이블이 즉시 갱신되도록 수정
- Run Demo 탐색 단계에서 실제 사용된 파라미터를 로그에 출력 (예: `Plane Fit Tolerance: 0.700mm`)

### 참고

- 별도 설치 불필요 — 압축 해제 후 `HoleBoltFinder.exe` 실행
- 이전 버전의 `presets.ini`는 최초 실행 시 자동 변환됩니다

### 시스템 요구사항

- Windows 10 64비트 이상

---

## HoleBoltFinder v1.1.3

### Improvements

- Restored Run Demo 3-step flow: (1) Load scene → (2) Move camera → (3) Search
  - Previously, loading a new file and moving the camera were combined into a single step
  - Now, loading a new file shows the full point cloud first, then the next click moves the camera, and the final click runs the search
  - For points using the same already-loaded file, the load step is skipped (2-step: camera → search)

### Notes

- No installation required — extract zip and run `HoleBoltFinder.exe`

### Requirements

- Windows 10 x64 or later

---

## HoleBoltFinder v1.1.3

### 개선사항

- Run Demo 3단계 흐름 복원: (1) 씬 로드 → (2) 카메라 이동 → (3) 탐색
  - 이전 버전에서 새 파일 로드와 카메라 이동이 하나의 단계로 합쳐져 있었던 것을 수정
  - 새 파일 로드 시 전체 포인트클라우드를 먼저 표시한 뒤, 다음 클릭에서 카메라가 이동하고, 마지막 클릭에서 탐색 실행
  - 이미 로드된 동일 파일을 사용하는 포인트의 경우 로드 단계 생략 (2단계: 카메라 이동 → 탐색)

### 참고

- 별도 설치 불필요 — 압축 해제 후 `HoleBoltFinder.exe` 실행

### 시스템 요구사항

- Windows 10 64비트 이상

---

## HoleBoltFinder v1.1.2

### Bug Fixes

- Fixed incorrect boundary check in the LZF decompressor that could cause `binary_compressed` PCD files to fail loading prematurely

### Notes

- No installation required — extract zip and run `HoleBoltFinder.exe`

### Requirements

- Windows 10 x64 or later

---

## HoleBoltFinder v1.1.2

### 버그 수정

- `binary_compressed` PCD 파일 로드 시 LZF 압축 해제 중 잘못된 경계 검사로 인해 로딩이 실패하는 문제 수정

### 참고

- 별도 설치 불필요 — 압축 해제 후 `HoleBoltFinder.exe` 실행

### 시스템 요구사항

- Windows 10 64비트 이상

---

## HoleBoltFinder v1.1.1

### Bug Fixes

- Fixed detection failure when loading unorganized PCD files (e.g. LiDAR scans where `HEIGHT=1`)
- Crop now rasterizes unorganized point clouds into a 2D grid based on XY world coordinates before detection

### Notes

- No installation required — extract zip and run `HoleBoltFinder.exe`

### Requirements

- Windows 10 x64 or later

---

## HoleBoltFinder v1.1.1

### 버그 수정

- 비정형(unorganized) PCD 파일 로드 시 검출 실패하는 문제 수정 (`HEIGHT=1`인 라이다 스캔 데이터 등)
- Crop 단계에서 비정형 포인트클라우드를 XY 좌표 기반 2D 격자로 래스터라이징 후 검출 처리

### 참고

- 별도 설치 불필요 — 압축 해제 후 `HoleBoltFinder.exe` 실행

### 시스템 요구사항

- Windows 10 64비트 이상

---

## HoleBoltFinder v1.0.0

### Features

- Point cloud visualization with OpenGL (color-mapped by Z height)
- Hole detection via RANSAC plane fitting + circle fitting
- Bolt height detection
- Preset management (save / load / add detection parameters)
- Demo mode with sample point cloud files
- PCD file support (ascii / binary formats)

### Notes

- Runs on machines without a dedicated GPU (VM and Windows Sandbox supported)
- No installation required — extract zip and run `HoleBoltFinder.exe`

### Requirements

- Windows 10 x64 or later

---

## HoleBoltFinder v1.0.0

### 주요 기능

- OpenGL 기반 포인트클라우드 시각화 (Z 높이 컬러맵)
- RANSAC 평면 추정 + 원 피팅을 통한 홀 검출
- 볼트 높이 검출
- Preset 관리 (저장 / 불러오기 / 추가)
- 샘플 파일을 이용한 데모 모드
- PCD 파일 지원 (ascii / binary 형식)

### 참고

- GPU 없는 환경(VM, Windows Sandbox) 자동 지원
- 별도 설치 불필요 — 압축 해제 후 `HoleBoltFinder.exe` 실행

### 시스템 요구사항

- Windows 10 64비트 이상

---

## HoleBoltFinder v0.10.0

### Bug Fixes

- Fixed a crash that occurred when opening a file that is not a point cloud
- File size and header are now validated before any memory allocation

### Notes

- No installation required — extract zip and run `HoleBoltFinder.exe`

### Requirements

- Windows 10 x64 or later

---

## HoleBoltFinder v0.10.0

### 버그 수정

- 포인트클라우드가 아닌 파일을 열었을 때 프로그램이 튕기는 문제 수정
- 메모리 할당 전 파일 크기 및 헤더 유효성 검사 추가

### 참고

- 별도 설치 불필요 — 압축 해제 후 `HoleBoltFinder.exe` 실행

### 시스템 요구사항

- Windows 10 64비트 이상

---

## HoleBoltFinder v0.9.0

First public release.

### Features

- Point cloud visualization with OpenGL (color-mapped by Z height)
- Hole detection via RANSAC plane fitting + circle fitting
- Bolt height detection
- Preset management (save / load / add detection parameters)
- Demo mode with sample point cloud files

### Notes

- Runs on machines without a dedicated GPU (VM and Windows Sandbox supported)
- No installation required — extract zip and run `HoleBoltFinder.exe`

### Requirements

- Windows 10 x64 or later

---

## HoleBoltFinder v0.9.0

첫 번째 공개 릴리스입니다.

### 주요 기능

- OpenGL 기반 포인트클라우드 시각화 (Z 높이 컬러맵)
- RANSAC 평면 추정 + 원 피팅을 통한 홀 검출
- 볼트 높이 검출
- Preset 관리 (저장 / 불러오기 / 추가)
- 샘플 파일을 이용한 데모 모드

### 참고

- GPU 없는 환경(VM, Windows Sandbox) 자동 지원
- 별도 설치 불필요 — 압축 해제 후 `HoleBoltFinder.exe` 실행

### 시스템 요구사항

- Windows 10 64비트 이상
