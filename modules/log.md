# Module fork changes

`master` = ModalAI mirror. Our work is on `main`.

## voxl-tflite-server

- Fixed a hang: worker threads could miss each other’s wake-up signal and stop
  publishing while still looking “alive”.
- Added shared depth helpers (undistort, preprocess, output format).
- Registered MiDaS, DepthAnythingV3, ZipDepth.
- Added **disparity** output pipe (`tflite_disparity`); depth no longer drops
  when nobody is on the image/detection pipes.
- Ships `undistort.yml` (must push/install; camera block is per-airframe —
  D0014 by default).

## voxl-open-vins-server

- Crash fix when tracking goes sparse (not enough points after optical flow →
  OpenCV throw).
- Based on open_vins **0.6.2** (correct 3D feature positions for the rescaler).
- Tuned Starling 2 feature yield in `estimator_config.yaml`:
  `num_pts` 50→200, `min_px_dist` 50→20, `max_msckf_in_update` 10→20,
  `max_slam_in_update` 15→25.

