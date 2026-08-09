# Deploy AI Models on ModalAI VOXL 2

Deploying TFLite models and the services around them on VOXL 2 (QRB5165).

```bash
git clone --recursive https://github.com/dineshvarunshankar/AI_VOXL2.git   # repo + pinned modules
cd AI_VOXL2
git lfs install && git lfs pull                                            # models in tflite/ are LFS-tracked
git submodule update --init --remote --recursive                           # optional: latest module mains
```

`--recursive` is required; Else `modules/` is empty. Without `git lfs
pull` the models in `tflite/` are pointer files. Skip the last command if the
pinned module commits are enough.

Commands are marked **[host]** for the build machine and **[drone]** for a shell
on the VOXL. Anything starting `adb` runs on the host and acts on the drone.

## Modules

- **`voxl-tflite-server`** : runs a TFLite model on a camera stream and
  publishes its output on an MPA pipe.
- **`voxl-open-vins-server`** : VIO. Publishes pose and 3D feature positions.
- **`mono_depth_rescaler`** : converts relative disparity to metric depth using
  VIO features and ToF as distance references. Runs no model itself.

Deploy only the modules you need. To add a model that isn't already registered,
see `mymodel/README.md`.

## Requirements

- VOXL 2 / QRB5165 connected over USB or WiFi
- Docker
- ModalAI `voxl-docker` wrapper and `voxl-cross` image

## Repo Layout

```text
AI_VOXL2/
├── modules/
│   ├── voxl-tflite-server/
│   ├── voxl-open-vins-server/
│   └── mono_depth_rescaler/
├── configs/              #drone configs, edited here and pushed
├── mymodel/              #template for a new model
└── tflite/               #model binaries
```

`configs/` holds drone configs that no module of ours packages. Paths mirror the
drone.

```text
configs/etc/modalai/   camera-server, qvio-server, imu-server, px4,
                       rangefinder-server, vio_cams, extrinsics
```

Configs belonging to our three modules live in those modules and ship in their
`.deb`. §5 pushes the rest; §6 checks both.

## 1. Install Build Tools

**[host]** `voxl-docker` runs the container with the right mounts. `voxl-cross`
is the image with the cross compiler and SDK dependencies.

```bash
mkdir -p ~/voxl2 && cd ~/voxl2
git clone https://gitlab.com/voxl-public/voxl-docker.git
cd voxl-docker
./install-voxl-docker-script.sh

docker load -i ~/Downloads/voxl-cross_V4.8.tgz
docker tag voxl-cross:V4.8 voxl-cross:latest
voxl-docker -l
```

Every module builds the same way: enter the container from the module directory,
install its dependencies, build, package.

`dev` is the package section, except VIO — see §3. Check library versions match
the drone: `dpkg -l | grep libmodal`.

## 2. voxl-tflite-server

### Build **[host]**

```bash
cd modules/voxl-tflite-server
voxl-docker -i voxl-cross
./install_build_deps.sh qrb5165 dev
./build.sh qrb5165          # JOBS=2 ./build.sh qrb5165 if it OOMs
./make_package.sh
exit
```

### Deploy **[host]**

```bash
./deploy_to_voxl.sh adb          # or: ./deploy_to_voxl.sh ssh <ip>
adb push ../../tflite/MiDaS/midas.tflite /usr/bin/dnn/midas.tflite
```

### Configure

The `.deb` carries `voxl-tflite-server.conf`, which sets model path,
architecture, input pipe and delegate. `voxl-configure-tflite` writes the same
file if you prefer flags.

- `model_architecture` selects the helper; see §8 for what's registered
- `delegate`: `gpu`, `nnapi`, or `cpu`
- `skip_n_frames` throttles inference. The model runs at roughly 15 fps, so use
  `0` when the camera is at 15 fps and `1` when it is at 30
- `input_pipe`: Starling 2 uses `hires_small_color`, Starling 2 MAX uses
  `hires_front_small_color`

For depth models, `undistort.yml` sets `publish_image: 0` and
`publish_disparity: 1`. The package installs it; its `camera` block is D0014's
calibration and must be replaced for another airframe. The output pipe is
`tflite_disparity`, or `{prefix}_tflite_disparity` when `allow_multiple: true`.

### Verify **[drone]**

```bash
systemctl restart voxl-tflite-server
voxl-inspect-cam tflite_disparity
```

## 3. voxl-open-vins-server

### Build **[host]**

```bash
cd modules/voxl-open-vins-server
voxl-docker -i voxl-cross
./install_build_deps.sh qrb5165 sdk-1.6   # dev has no libmodal-flow 1.0.3
./build.sh qrb5165          # JOBS=2 ./build.sh qrb5165 if it OOMs
./make_package.sh
exit
```

### Deploy **[host]**

```bash
./deploy_to_voxl.sh adb
```

The `.deb` carries `estimator_config.yaml` and `voxl-open-vins-server.conf` with
the tuned values. Confirm them after install, per §6.

### Configure

Three files configure it:

- `/etc/modalai/voxl-open-vins-server.conf` : service behaviour, auto-reset
  thresholds, and `yaml_folder` pointing at the platform config
- `/etc/modalai/vio_cams.conf` : which camera pipes feed VIO
- `<yaml_folder>/estimator_config.yaml` : the estimator, including feature yield
  (`num_pts`, `min_px_dist`, `max_msckf_in_update`, `max_slam_in_update`)

`voxl-configure-open-vins starling2_2cam` **[drone]** regenerates the first two
from the Starling 2 preset.

### Verify **[drone]**

```bash
systemctl restart voxl-open-vins-server
voxl-inspect-vins
```

`state` should reach `OKAY` and stay there with features well above zero.
Initialisation needs motion; a stationary drone sits in `INIT`.

## 4. mono_depth_rescaler

### Build **[host]**

```bash
cd modules/mono_depth_rescaler
voxl-docker -i voxl-cross
./install_build_deps.sh qrb5165 dev
./build.sh qrb5165          # JOBS=2 ./build.sh qrb5165 if it OOMs
./make_package.sh
exit
```

`./build.sh native` builds on the host and runs the tests.

### Deploy **[host]**

```bash
./deploy_to_voxl.sh adb          # or: ./deploy_to_voxl.sh ssh <ip>
```

The `.deb` carries `pipeline.yaml`, intrinsics and extrinsics into
`/etc/mono_depth_rescaler`. Edit them in `modules/mono_depth_rescaler/config/`
and rebuild, or push one file for a quick change:

```bash
adb push config/pipeline.yaml /etc/mono_depth_rescaler/pipeline.yaml
```

### Configure

`pipeline.yaml` holds every knob.

```yaml
deployment:
  profile: qvio      # or openvins
inference:
  fov: crop
  input_resolution: [256, 256]
  mpa_pipe_name: tflite_disparity
```

Enable the matching VIO service. The `inference` block must agree with the tflite
side or frames are dropped:

| Side | Setting |
| --- | --- |
| `voxl-tflite-server.conf` | `allow_multiple: false`, input `hires_small_color` |
| `undistort.yml` | `publish_image: 0`, `publish_disparity: 1`, `fov: crop` |
| `pipeline.yaml` | `mpa_pipe_name: tflite_disparity`, `fov: crop`, `input_resolution: [256, 256]` |

### Verify **[drone]**

```bash
systemctl start mono_depth_rescaler
systemctl is-active mono_depth_rescaler
voxl-inspect-cam metric_depth
```

## 5. Push Configs **[host]**

`voxl-camera-server` is not one of our modules, so its config is pushed. Run
from the repo root.

```bash
adb push configs/etc/modalai/voxl-camera-server.conf /etc/modalai/voxl-camera-server.conf
adb shell systemctl restart voxl-camera-server
```

The rest of `configs/etc/modalai/` is ModalAI defaults, kept for reference.

## 6. Configs Carried by a Package

A reinstall overwrites these.

```bash
grep -E "num_pts|max_cameras|calib_cam_|min_px_dist|max_msckf_in_update|max_slam_in_update" \
  /usr/share/modalai/voxl-open-vins/VoxlConfig/starling2/estimator_config.yaml
grep -E "anchor_cone" /etc/modalai/voxl-open-vins-server.conf
cat /etc/modalai/voxl-tflite-server.conf
grep -E "profile|min_quality|fov|input_resolution|mpa_pipe_name" /etc/mono_depth_rescaler/pipeline.yaml
grep -E "publish_image|publish_disparity|fov" /etc/voxl-tflite-server/undistort.yml
```

## 7. Debug **[drone]**

```bash
ls /run/mpa/
voxl-inspect-services
journalctl -b -u voxl-tflite-server --no-pager | tail -40
journalctl -b -u voxl-open-vins-server --no-pager | tail -40
strings /usr/bin/voxl-tflite-server | grep -E 'MIDAS|MidasModelHelper'
```

Per module:

```bash
voxl-inspect-cam tflite_disparity     # tflite: is disparity publishing
voxl-inspect-vins                     # VIO: state and feature count
voxl-inspect-cam metric_depth         # rescaler: is metric depth publishing
```

To see why VIO fails to start or publish:

```bash
systemctl stop voxl-open-vins-server && /usr/local/bin/voxl-open-vins-server -v -d
```

tflite timing mode, for per-stage timings and output rate:

```bash
systemctl stop voxl-tflite-server
/usr/bin/voxl-tflite-server -t
```

The board throttles hard without airflow. Above roughly 100 °C the GPU drops to
its lowest clock and VIO features collapse, which reads as a software fault:

```bash
voxl-inspect-cpu
```

MPA consumers reconnect on their own, so a camera or VIO restart does not require
restarting anything downstream.

## 8. Registered Models

Registered in the `voxl-tflite-server` fork.

| Model | Input | Preprocessing | Output | `model_architecture` |
| --- | --- | --- | --- | --- |
| MiDaS | quantized `uint8` | RGB to `[0,1]`, then quantized | quantized or float depth | `MIDAS_V2` |
| DepthAnythingV3 | `1x518x518x3` float32 | `uint8 RGB / 255.0` | float32 depth | `DEPTHANYTHINGV3` |
| ZipDepth | `1x384x384x3` float32 | `uint8 RGB / 255.0` | float32 inverse-depth | `ZIPDEPTH` |

All are `MONO_DEPTH` and run on the `gpu` delegate. Each can publish two pipes
(see `undistort.yml`): **image** (JET colormap viz) and **disparity** (float32
for the rescaler, usually `tflite_disparity`). Binaries are under
`tflite/<ModelName>/`.

## References

- [voxl-tflite-server](https://docs.modalai.com/voxl-tflite-server/)
- [Open-VINS server](https://docs.modalai.com/voxl-open-vins-server/)
- [Qualcomm AI Hub](https://aihub.qualcomm.com/models)

Fork changes for the modules live in [`modules/log.md`](modules/log.md).
