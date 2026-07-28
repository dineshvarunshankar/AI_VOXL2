# Deploy AI Models on ModalAI VOXL 2

This repository helps you deploy custom TFLite models on ModalAI's Qualcomm-based VOXL 2 AI computer.

The main idea:

- `mymodel/` is the reusable template.
- `examples/` contains filled-in model helpers.
- `voxl-tflite-server` is a separate ModalAI repo that you patch, build, and install on VOXL 2.

## Requirements

- VOXL 2 / QRB5165 connected over USB or WiFi
- Linux PC with Docker
- ModalAI `voxl-docker` wrapper
- ModalAI `voxl-cross` Docker image
- A TFLite model and its input/output details

## Repo Layout

```text
AI_VOXL2/
├── depth_utils/
│   ├── include/
│   └── src/
├── mymodel/
│   ├── include/model_helper/mymodel_model_helper.h
│   └── src/model_helper/mymodel_model_helper.cpp
├── examples/
│   ├── MiDaS/
│   ├── DepthAnythingV3/
│   └── ZipDepth/
├── patches/
│   └── fix-postprocess-deadlock.patch
└── tflite/
    ├── MiDaS/
    ├── DepthAnythingV3/
    └── ZipDepth/
```

Use `mymodel/` when starting a new model. Use `examples/` for concrete references.
Model binaries live under `tflite/<ModelName>/` (`.tflite` files are gitignored).
Depth helpers also need `depth_utils/` copied into `voxl-tflite-server`.

## 1. Prepare a TFLite Model

You can:

- use your own TFLite model,
- download a suitable model from [Qualcomm AI Hub](https://aihub.qualcomm.com/models), or
- convert a PyTorch model to ONNX and then to TFLite.

Before writing the helper, know the model contract:

- input shape, for example `1x256x256x3` or `1x3x518x518`
- layout, usually NHWC for `voxl-tflite-server`
- dtype, such as `uint8`, `int8`, or `float32`
- quantization scale and zero point if quantized
- output shape and meaning, such as depth, mask, logits, or boxes

If you use Qualcomm AI Hub, `metadata.json` is a useful reference for shape, dtype, and quantization constants. Use a device profile close to QRB5165.

## 2. Understand the Template

The template files are:

- `mymodel/include/model_helper/mymodel_model_helper.h`
- `mymodel/src/model_helper/mymodel_model_helper.cpp`

Rename `mymodel` / `MyModel` / `MYMODEL` when creating a real model helper.

The helper has four parts:

1. **Constructor**: calls `ModelHelper(...)`. The base class loads the `.tflite`, builds the interpreter, and reads `model_height`, `model_width`, and `model_channels` from the input tensor.
2. **`run_inference`**: writes `preprocessed_image` into the model input tensor, applies dtype/layout/quantization handling, then calls `Invoke()`.
3. **`postprocess`**: reads the output tensor and turns it into what you want to publish, such as a depth colormap, mask, detections, logits, or embeddings.
4. **`worker`**: calls `postprocess` and publishes to the MPA pipe(s).
   `timestamp_ns` must stay as the camera capture time. Replacing it with
   `rc_nanos_monotonic_time()` stamps the frame at publish time instead, so
   anything syncing against IMU/VIO is offset by inference latency.

The `.tflite` file does not tell the server which algorithm to run. The helper class does that. The config value `model_architecture` selects the helper at runtime.

## 3. Install ModalAI Build Tools

`voxl-docker` is the host-side wrapper that runs the Docker container with the right mounts. `voxl-cross` is the Docker image that contains the cross compiler and VOXL SDK dependencies.

Install `voxl-docker`:

```bash
mkdir -p ~/voxl2 && cd ~/voxl2
git clone https://gitlab.com/voxl-public/voxl-docker.git
cd voxl-docker
./install-voxl-docker-script.sh
```

Load `voxl-cross`:

```bash
docker load -i ~/Downloads/voxl-cross_V4.8.tgz
docker tag voxl-cross:V4.8 voxl-cross:latest
voxl-docker -l
```

## 4. Patch `voxl-tflite-server`

Clone the ModalAI server repo into a path without spaces:

```bash
cd ~/voxl2
git clone https://gitlab.com/voxl-public/voxl-sdk/services/voxl-tflite-server.git
cd voxl-tflite-server
```

Apply the upstream fix:

```bash
export AI_VOXL2=/path/to/AI_VOXL2
git apply "$AI_VOXL2"/patches/fix-postprocess-deadlock.patch
```

Without it the server can stop producing output after a random number of
frames. The process stays alive and keeps logging, but the output pipe goes
quiet until it is restarted.

Copy the template helper into the server tree:

```bash
cp "$AI_VOXL2"/mymodel/include/model_helper/*.h include/model_helper/
cp "$AI_VOXL2"/mymodel/src/model_helper/*.cpp src/model_helper/
```

For a depth example, copy from `examples/MiDaS/`, `examples/DepthAnythingV3/`,
or `examples/ZipDepth/` instead of `mymodel/`, and also copy depth utils:

```bash
mkdir -p src/depth_utils include/depth_utils
cp "$AI_VOXL2"/depth_utils/include/*.h include/depth_utils/
cp "$AI_VOXL2"/depth_utils/src/*.cpp   src/depth_utils/
```

After copying, the relevant server files are:

```text
voxl-tflite-server/
├── include/
│   ├── depth_utils/
│   │   ├── undistort_map.h
│   │   ├── depth_preprocessor.h
│   │   └── depth_output.h
│   └── model_helper/
│       ├── model_info.h
│       ├── model_helper.h
│       └── midas_model_helper.h
└── src/
    ├── main.cpp
    ├── depth_utils/
    │   ├── undistort_map.cpp
    │   ├── depth_preprocessor.cpp
    │   └── depth_output.cpp
    └── model_helper/
        ├── model_helper.cpp
        └── midas_model_helper.cpp
```

### 4.1 Edit `model_info.h`

Add your enum value to `ModelName`:

```cpp
enum ModelName
{
    ...
    MIDAS,
    MYMODEL,
    DEEPLAB,
    ...
};
```

### 4.2 Edit `model_helper.cpp`

Include your helper:

```cpp
#include "model_helper/mymodel_model_helper.h"
```

Add a factory case in `create_model_helper()`:

```cpp
case MYMODEL:
{
    if (model_category == MONO_DEPTH)
    {
        return new MyModelModelHelper(model, labels_in_use, opt_, en_debug, en_timing,
                                      do_normalize);
    }
    fprintf(stderr, "Unsupported category for the given model\n");
    break;
}
```

Choose the right category for your model: `OBJECT_DETECTION`, `CLASSIFICATION`, `SEGMENTATION`, `MONO_DEPTH`, or `POSE`.

### 4.3 Edit `main.cpp`

Map the config string to your enum and category inside `get_model_type()`:

```cpp
else if (!strcasecmp(model_architecture, "MYMODEL"))
{
    *model_name = MYMODEL;
    *model_category = MONO_DEPTH;
}
```

Also add `MYMODEL` to the valid-options error message in the same function.

### 4.4 Depth models: pipes and undistort

`undistort.yml`:

```yaml
publish_image: 0
publish_disparity: 1
```

`include/model_helper/model_helper.h`:

```cpp
#define IMAGE_CH 0
#define DETECTION_CH 1
#define DISPARITY_CH 2
#define TFLITE_DISPARITY_PATH (MODAL_PIPE_DEFAULT_BASE_DIR "tflite_disparity/")
```

`main.cpp` — create `DISPARITY_CH` in both `!allow_multiple` and `allow_multiple`.
For `publish_image: 0`, create only `DISPARITY_CH` for `MONO_DEPTH`.

```cpp
// !allow_multiple
pipe_info_t disparity_pipe = {
    "tflite_disparity", TFLITE_DISPARITY_PATH, "camera_image_metadata_t",
    PROCESS_NAME, 16 * 1024 * 1024, 0};
pipe_server_create(DISPARITY_CH, disparity_pipe, 0);

// allow_multiple
pipe_info_t disparity_pipe = {
    "tflite_disparity", "unknown", "camera_image_metadata_t",
    PROCESS_NAME, 16 * 1024 * 1024, 0};
std::string disp = MODAL_PIPE_DEFAULT_BASE_DIR;
disp.append(output_pipe_prefix);
disp.append("_tflite_disparity");
strncpy(disparity_pipe.location, disp.c_str(), MODAL_PIPE_MAX_DIR_LEN - 1);
disparity_pipe.location[MODAL_PIPE_MAX_DIR_LEN - 1] = '\0';
pipe_server_create(DISPARITY_CH, disparity_pipe, 0);
```

Pipe names: `tflite_disparity`, or `{prefix}_tflite_disparity`.

`_camera_helper_cb` cannot see `main`'s local `model_category`. Store it and skip
the client gate for depth:

```cpp
// file scope (near other globals)
static ModelCategory g_model_category;

// in main(), after get_model_type(...):
g_model_category = model_category;

// in _camera_helper_cb, replace the stock client check with:
if (!en_debug && !en_timing && g_model_category != MONO_DEPTH)
{
    if (!pipe_server_get_num_clients(IMAGE_CH) &&
        !pipe_server_get_num_clients(DETECTION_CH))
        return;
}
```

Copy source `camera_image_metadata_t` before `postprocess()` if `DISPARITY_CH`
needs the capture timestamp. `K_model` is printed at helper startup.

**Starling 2 + mono_depth_rescaler (MiDaS):** match both sides and keep FLOAT32.

| Side | Setting |
| --- | --- |
| tflite conf | `model_architecture` = your enum (e.g. `MIDAS_V2`), `delegate: gpu`, `allow_multiple: false`, `skip_n_frames: 1`, input `hires_small_color` |
| `undistort.yml` | `publish_image: 0`, `publish_disparity: 1`, `fov: crop` |
| rescaler | `mpa_pipe_name: tflite_disparity`, `fov: crop`, `input_resolution: [256, 256]`, same hires intrinsics |

Wrong FOV/size leads to dropped frames. Pipe name is `tflite_disparity` only when `allow_multiple: false`.

```bash
adb push depth_utils/undistort.yml /etc/voxl-tflite-server/undistort.yml
voxl-logger --raw tflite_disparity
```

## 5. Cross-Compile

Enter the build container:

```bash
cd ~/voxl2/voxl-tflite-server
voxl-docker -i voxl-cross
```

Inside the container:

```bash
./install_build_deps.sh qrb5165 dev
# or qrb5165-2 if your VOXL 2 image is Ubuntu 20.04 / SDK 2.x

./build.sh qrb5165
# or ./build.sh qrb5165-2
```

If a build fails after changing C++ files, clean and rebuild:

```bash
rm -rf build
./build.sh qrb5165
```

Package the build:

```bash
./make_package.sh
exit
```

The package appears in the `voxl-tflite-server` repo root as `voxl-tflite-server_*_arm64.deb`.

## 6. Deploy to VOXL 2

From the host, install the `.deb` on VOXL 2.

USB / ADB:

```bash
cd ~/voxl2/voxl-tflite-server
./deploy_to_voxl.sh adb
```

WiFi / SSH:

```bash
cd ~/voxl2/voxl-tflite-server
./deploy_to_voxl.sh ssh <ip>
```

Optional cleanup and binary check:

```bash
adb shell "rm -f /data/voxl-tflite-server_*_arm64.deb"
adb shell "strings /usr/bin/voxl-tflite-server | grep -E 'MYMODEL|MyModelModelHelper' | head -5"
```

For SSH:

```bash
ssh root@${VOXL_IP} "rm -f /data/voxl-tflite-server_*_arm64.deb /tmp/voxl-tflite-server_*_arm64.deb"
ssh root@${VOXL_IP} "strings /usr/bin/voxl-tflite-server | grep -E 'MYMODEL|MyModelModelHelper' | head -5"
```

## 7. Install the Model File

Copy the `.tflite` model to VOXL 2. The path is your choice; the config must match it.

ADB:

```bash
adb push mymodel.tflite /usr/bin/dnn/mymodel.tflite
```

SSH:

```bash
scp mymodel.tflite root@<IP>:/usr/bin/dnn/mymodel.tflite
```

## 8. Configure and Run

`voxl-configure-tflite` writes `/etc/modalai/voxl-tflite-server.conf`. The two important fields are:

- `model`: full path to the `.tflite` file
- `model_architecture`: helper string handled in `main.cpp`

Configure your model:

```bash
voxl-configure-tflite \
  --model-path /usr/bin/dnn/mymodel.tflite \
  --model-arch MYMODEL \
  --norm-type NONE \
  --input-pipe /run/mpa/hires_small_color/ \
  --delegate gpu \
  --output-pipe-prefix MYMODEL \
  --require-labels false \
  --skip-frames 0
```

Config notes:

- `--norm-type NONE`: use this when your helper already normalizes inside `run_inference()`.
- `--delegate gpu`: runs supported graph partitions with the TFLite GPU delegate.
- `--delegate nnapi`: tries NNAPI/NPU-style acceleration on QRB5165 builds.
- `--delegate cpu`: uses CPU/XNNPACK; useful for debugging and as a stable baseline.
- `--skip-frames 0`: process every camera frame (use for MiDaS ↔ rescaler). Raise only to throttle load when hot.
- `--input-pipe`: Starling 2 → `hires_small_color`; Starling 2 MAX → `hires_front_small_color`.

Restart the service:

```bash
systemctl restart voxl-tflite-server
```

Verify:

```bash
cat /etc/modalai/voxl-tflite-server.conf
journalctl -u voxl-tflite-server --since "5 min ago" --no-pager
ls -la /run/mpa/tflite/
```

For visualization, open `voxl-portal` and select the `tflite` camera stream. For verification and performance testing, prefer `voxl-inspect-cam tflite` and timing mode.

## 9. Debug
- Confirm config
```bash
cat /etc/modalai/voxl-tflite-server.conf
```
- Confirm Binary has your helper
```bash
strings /usr/bin/voxl-tflite-server | grep -E 'YOUR_ARCH|YourModelHelper'
```
- Confirm model file exists
```bash
ls -lh /usr/bin/dnn/your_model.tflite
```
- Confirm camera pipe exists
```bash
ls /run/mpa/
ls /run/mpa/hires_front_small_color/
```
- Inspect output pipe
```bash
voxl-inspect-cam tflite
```
- Timing test
```bash
systemctl stop voxl-tflite-server
sudo killall voxl-tflite-server 2>/dev/null
ps aux | grep '[v]oxl-tflite-server' || echo "OK"
/usr/bin/voxl-tflite-server -t
```
For output FPS, look for a log line like `Current pipeline throughput: 2.5 frames per second`. The timing table is mainly useful for rough stage timing; depending on the helper, its `processed frames` count may not equal completed published output frames.

## 10. Examples

These are complete helper examples you can copy into `voxl-tflite-server` instead of starting from `mymodel/`.
Depth examples also need `depth_utils/` (which includes the shared `undistort.yml`, see §4.4).

### 10.1 DepthAnythingV3

Path:

```text
examples/DepthAnythingV3/
```

This helper expects a float32 DA3 export:

- input: NHWC RGB, `1x518x518x3`
- input preprocessing: `uint8 RGB / 255.0`
- output: float32 depth map
- visualization: per-frame normalized JET colormap
- suggested `model_architecture`: `DEPTHANYTHINGV3`
- category: `MONO_DEPTH`
- model: `tflite/DepthAnythingV3/depth_anything_v3.tflite`

### 10.2 MiDaS

Path:

```text
examples/MiDaS/
```

This helper is based on the Qualcomm AI Hub MiDaS w8a8 export:

- input: quantized `uint8`
- input preprocessing: RGB scaled to `[0,1]`, then quantized using `kMidasInScale` and `kMidasInZeroPoint`
- output: quantized or float depth map
- visualization: per-frame normalized JET colormap
- suggested `model_architecture`: `MIDAS`
- category: `MONO_DEPTH`
- model: `tflite/MiDaS/midas.tflite`

### 10.3 ZipDepth

Path:

```text
examples/ZipDepth/
```

This helper expects a float32 ZipDepth export:

- input: NHWC RGB, `1x384x384x3`
- input preprocessing: `uint8 RGB / 255.0`
- output: float32 inverse-depth
- visualization: per-frame normalized JET colormap
- suggested `model_architecture`: `ZIPDEPTH`
- category: `MONO_DEPTH`
- suggested delegate: `gpu`
- model: `tflite/ZipDepth/zipdepth_base_384x384_float32.tflite`

## References

- [voxl-tflite-server](https://docs.modalai.com/voxl-tflite-server/)
- [Qualcomm AI Hub](https://aihub.qualcomm.com/models)
