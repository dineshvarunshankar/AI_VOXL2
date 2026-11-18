# Module Fork Changes

ModalAI forks use `master` for the upstream mirror and `main` for local changes.

## voxl-tflite-server

- Fixed missed worker wake-up signals.
- Added shared depth preprocessing and output helpers.
- Registered MiDaS, DepthAnythingV3, and ZipDepth.
- Added the `tflite_disparity` pipe and inference without image-pipe clients.
- Added `undistort.yml` with D0014 camera calibration defaults.

## voxl-open-vins-server

- Added a sparse-tracking RANSAC guard in the `open_vins` submodule.
- Uses server version 0.6.2 with corrected anchored-feature positions.
- Set Starling 2 values in `estimator_config.yaml`:
  `num_pts: 250`, `min_px_dist: 20`, `max_msckf_in_update: 20`, and `max_slam_in_update: 25`.
