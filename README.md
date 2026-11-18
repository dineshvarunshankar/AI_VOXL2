# AI Deployment on VOXL 2

Deploy monocular depth models on ModalAI VOXL 2 and recover metric depth from VIO and ToF measurements.

Platform: QRB5165, voxl-suite `1.6.3~beta8`, voxl-cross `V4.8`.
Host commands use Docker and ADB. Drone commands run in a VOXL shell.

## 1. Setup

On the host, install Git LFS and Docker, then clone:

```bash
git lfs install
git clone --recursive https://github.com/dineshvarunshankar/AI_VOXL2.git
cd AI_VOXL2
git lfs pull
export AI_VOXL2_ROOT="$PWD"
```

Download `voxl-cross_V4.8.tgz` from ModalAI's developer downloads. Install the build wrapper:

```bash
(
  mkdir -p ~/voxl2
  cd ~/voxl2
  git clone https://gitlab.com/voxl-public/voxl-docker.git
  cd voxl-docker
  ./install-voxl-docker-script.sh
)
docker load -i ~/Downloads/voxl-cross_V4.8.tgz
docker tag voxl-cross:V4.8 voxl-cross:latest
```

## 2. Build and Deploy

Run these commands on the host. Commands between `voxl-docker` and `exit` run inside the container.

### TFLite server

```bash
cd "$AI_VOXL2_ROOT/modules/voxl-tflite-server"
voxl-docker -i voxl-cross
./install_build_deps.sh qrb5165 dev
./build.sh qrb5165
./make_package.sh
exit
./deploy_to_voxl.sh adb
adb push "$AI_VOXL2_ROOT/tflite/MiDaS/midas.tflite" /usr/bin/dnn/midas.tflite
adb push "$AI_VOXL2_ROOT/tflite/ZipDepth/zipdepth_npu.tflite" /usr/bin/dnn/zipdepth_npu.tflite
```

### OpenVINS server

```bash
cd "$AI_VOXL2_ROOT/modules/voxl-open-vins-server"
voxl-docker -i voxl-cross
./install_build_deps.sh qrb5165 sdk-1.6
./build.sh qrb5165
./make_package.sh
exit
./deploy_to_voxl.sh adb
```

### Metric depth rescaler

```bash
cd "$AI_VOXL2_ROOT/modules/mono_depth_rescaler"
voxl-docker -i voxl-cross
./install_build_deps.sh qrb5165 dev
./build.sh qrb5165
./make_package.sh
exit
./deploy_to_voxl.sh adb
```

## 3. Configure

The packages install their service configuration and calibration files.
Replace D0014 camera calibration with calibration for your airframe.

| File on the drone | Settings |
|---|---|
| `/etc/modalai/voxl-tflite-server.conf` | Model path, architecture, delegate, input pipe |
| `/etc/voxl-tflite-server/undistort.yml` | Camera calibration, FOV, image and disparity outputs |
| `/etc/modalai/voxl-open-vins-server.conf` | VIO service and estimator configuration path |
| `/usr/share/modalai/voxl-open-vins/VoxlConfig/starling2/estimator_config.yaml` | Feature tracking and estimator parameters |
| `/etc/mono_depth_rescaler/pipeline.yaml` | VIO profile, disparity input, output size and format |

For the Starling 2 depth pipeline, use:

| Configuration | Values |
|---|---|
| TFLite server | `allow_multiple: false`, `input_pipe: hires_small_color` |
| Undistortion | `publish_image: 0`, `publish_disparity: 1`, `fov: crop` |
| Rescaler | `profile: openvins`, `mpa_pipe_name: tflite_disparity`, `fov: crop` |
| Depth output | `size: 0`, `format: uint16_mm`, `viz: 0` |

`size: 0` preserves model resolution. `uint16_mm` outputs millimetres; `float32` outputs metres.
Package installation replaces packaged configuration files.

Push the separate drone configuration from the host:

```bash
cd "$AI_VOXL2_ROOT"
adb push configs/etc/modalai/voxl-camera-server.conf /etc/modalai/voxl-camera-server.conf
adb push configs/etc/modalai/voxl-imu-server.conf /etc/modalai/voxl-imu-server.conf
adb push configs/etc/modalai/vio_cams.conf /etc/modalai/vio_cams.conf
adb push configs/etc/modalai/voxl-cpu-monitor.conf /etc/modalai/voxl-cpu-monitor.conf
adb shell systemctl restart voxl-camera-server voxl-imu-server voxl-cpu-monitor voxl-open-vins-server
```

These files select 15 fps hires capture, 200 Hz IMU sampling and polling, `tracking_front` VIO, and the `auto` pitmode governor.
Other files in `configs/etc/modalai/` are reference configurations.

## 4. Run

On the drone:

```bash
systemctl restart voxl-tflite-server voxl-open-vins-server
systemctl start mono_depth_rescaler
voxl-inspect-cam tflite_disparity
voxl-inspect-vins
voxl-inspect-cam metric_depth
```

Run each inspection command separately. VIO initialization requires motion; its running state is `OKAY`.
Provide board cooling during operation. Use `voxl-inspect-cpu` to check temperature and clocks.

## 5. Models

| Model | Input tensor | `model_architecture` |
|---|---|---|
| MiDaS | `1x256x256x3`, uint8 | `MIDAS_V2` |
| DepthAnythingV3 | `1x518x518x3`, float32 | `DA3` |
| ZipDepth | `1x384x384x3`, float32 | `ZIP_DEPTH` |

The helpers publish float32 relative inverse depth on `tflite_disparity`. Larger values represent nearer surfaces.
Model binaries are in `tflite/`. See [Adding a model](mymodel/README.md) and [Module changes](modules/log.md).

## References

- [VOXL TFLite server](https://docs.modalai.com/voxl-tflite-server/)
- [VOXL OpenVINS server](https://docs.modalai.com/voxl-open-vins-server/)
- [Qualcomm AI Hub](https://aihub.qualcomm.com/models)
