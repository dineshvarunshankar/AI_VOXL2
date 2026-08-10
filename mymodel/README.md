# Adding a Model

How to deploy your own model on `voxl-tflite-server`.

## 1. What you need to know about the model

Before writing code, collect:

- Input shape (e.g. `1x256x256x3` or `1x3x518x518`)
- Layout (usually NHWC on TFLite)
- Input dtype (`uint8`, `int8`, or `float32`)
- If quantized: scale and zero-point
- Output shape and what the values mean (depth, logits, boxes, mask, …)

You can get that from Netron, your export scripts, or the model card. If the
model came from [Qualcomm AI Hub](https://aihub.qualcomm.com/models), use its
`metadata.json`, and pick a device profile close to QRB5165 when exporting.

For a depth model, publish inverse depth on the disparity pipe — larger means
nearer. Invert in the helper if the model outputs true depth, or
`mono_depth_rescaler` fits the curve backwards.

## 2. Start from the template

Files:

- `include/model_helper/mymodel_model_helper.h`
- `src/model_helper/mymodel_model_helper.cpp`

Rename `mymodel` / `MyModel` / `MYMODEL` everywhere to your model name.

The helper has four pieces you fill in:

1. **Constructor**  
   Calls `ModelHelper(...)`. The base class loads the `.tflite`, builds the
   interpreter, and fills `model_height`, `model_width`, `model_channels` from
   the input tensor.

2. **`run_inference`**  
   Copy `preprocessed_image` into the input tensor (respect dtype / layout /
   quantization), then `Invoke()`.

3. **`postprocess`**  
   Read the output tensor and turn it into whatever you will publish.

4. **`worker`**  
   Call `postprocess` and publish on the MPA pipe(s). Keep `timestamp_ns` as
   the camera capture time. Do not replace it with
   `rc_nanos_monotonic_time()` unless you mean to; that stamps publish time and
   skews anything synced to IMU/VIO by the inference latency.

Look at the existing helpers under
`modules/voxl-tflite-server/src/model_helper/` (MiDaS, ZipDepth, YOLO, …) and
copy the pattern that matches your category.

Config side: put the `.tflite` path in `voxl-tflite-server.conf`, and set
`model_architecture` to the string you register in `get_model_type()` below so
the server constructs your helper.

## 3. Copy into the server tree

```bash
cd modules/voxl-tflite-server
cp ../../mymodel/include/model_helper/*.h include/model_helper/
cp ../../mymodel/src/model_helper/*.cpp   src/model_helper/
```

## 4. Register it

**`include/model_helper/model_info.h`** — add an enum value:

```cpp
enum ModelName
{
    ...
    MYMODEL,
    ...
};
```

**`src/model_helper/model_helper.cpp`** — include the header and add a factory
case in `create_model_helper()`:

```cpp
#include "model_helper/mymodel_model_helper.h"

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

Pick the category that fits: `OBJECT_DETECTION`, `CLASSIFICATION`,
`SEGMENTATION`, `MONO_DEPTH`, or `POSE`.

**`src/main.cpp`** — in `get_model_type()`, map your config string and list it
in the valid-options error text:

```cpp
else if (!strcasecmp(model_architecture, "MYMODEL"))
{
    *model_name = MYMODEL;
    *model_category = MONO_DEPTH;
}
```

That string is what you put in `"model_architecture"` in
`/etc/modalai/voxl-tflite-server.conf`.

## 5. Build and deploy

Follow the main README (`voxl-tflite-server` build / package / push).

For depth models, also set `undistort.yml` (`publish_image` /
`publish_disparity`, FOV, camera calibration) as described there. The rescaler
expects the disparity pipe (`tflite_disparity` when `allow_multiple` is false).
