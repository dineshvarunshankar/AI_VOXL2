# Add a Model

## 1. Check the tensor format

Record the input shape, layout, dtype, normalization, and quantization scale and zero-point.
Record the output shape, dtype, and value meaning.

The base helper assumes NHWC input, such as `1x256x256x3`.
For NCHW input, update the helper dimensions and preprocessing before inference.

For depth models, publish inverse depth on the disparity pipe. Larger values mean nearer surfaces.

## 2. Copy and complete the template

From the AI_VOXL2 repository root:

```bash
cp mymodel/include/model_helper/*.h modules/voxl-tflite-server/include/model_helper/
cp mymodel/src/model_helper/*.cpp modules/voxl-tflite-server/src/model_helper/
```

Rename `mymodel`, `MyModel`, and `MYMODEL` to your model name.

| Method | Required implementation |
| --- | --- |
| Constructor | Initialize `ModelHelper` and any model-specific dimensions. |
| `run_inference` | Copy the input with the correct dtype, layout, and quantization. Call `Invoke()`. |
| `postprocess` | Decode the output tensor and set the output metadata. |
| `worker` | Publish the output on the correct MPA pipe. Preserve the camera timestamp and exposure. |

Use the [existing helpers](../modules/voxl-tflite-server/src/model_helper/) as examples.

## 3. Register the helper

Add an enum value in `include/model_helper/model_info.h`:

```cpp
enum ModelName
{
    ...
    MYMODEL,
    ...
};
```

In `src/model_helper/model_helper.cpp`, include the header and add a case to `create_model_helper()`:

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

Select the model category: `OBJECT_DETECTION`, `CLASSIFICATION`, `SEGMENTATION`, `MONO_DEPTH`, or `POSE`.

In `src/main.cpp`, add the configuration string to `get_model_type()` and its valid-options message:

```cpp
else if (!strcasecmp(model_architecture, "MYMODEL"))
{
    *model_name = MYMODEL;
    *model_category = MONO_DEPTH;
}
```

## 4. Build and configure

Follow the [server build and deployment instructions](../README.md).

Set the model file path and `model_architecture` in `/etc/modalai/voxl-tflite-server.conf`.

For depth models, configure `undistort.yml` with the camera calibration, FOV, and output flags.
With `allow_multiple: false`, the disparity pipe is `tflite_disparity`.
