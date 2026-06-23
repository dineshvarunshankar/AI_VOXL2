#include "model_helper/midas_model_helper.h"

#include <cmath>
#include <cstring>

#include "image_utils.h"
#include "tensor_data.h"

// Publish the raw quantized MiDaS output (RAW8, lossless) for metric-depth
// rescaling instead of the RGB JET visualization. The host recovers relative
// disparity as: disparity = pixel * kMidasOutScale. Set to 0 for the colormap.
#define MIDAS_PUBLISH_RAW 1

MidasModelHelper::MidasModelHelper(char *model_file, char *labels_file,
                                   DelegateOpt delegate_choice, bool _en_debug,
                                   bool _en_timing, NormalizationType _do_normalize)
    : ModelHelper(model_file, labels_file, delegate_choice, _en_debug, _en_timing,
                  _do_normalize)
{
}

bool MidasModelHelper::run_inference(cv::Mat &preprocessed_image,
                                     double *last_inference_time)
{
    start_time = rc_nanos_monotonic_time();

    const int input_idx = interpreter->inputs()[0];
    TfLiteTensor *input_tensor = interpreter->tensor(input_idx);

    if (input_tensor->type != kTfLiteUInt8)
    {
        fprintf(stderr,
                "MidasModelHelper: expected uint8 input tensor, got type %d\n",
                input_tensor->type);
        fprintf(stderr,
                "Use midas-tflite-w8a8 from Qualcomm Hub, or add float I/O handling.\n");
        return false;
    }

    uint8_t *dst = TensorData<uint8_t>(input_tensor, 0);
    const int row_elems = model_width * model_channels;

    for (int row = 0; row < model_height; row++)
    {
        const uchar *row_ptr = preprocessed_image.ptr(row);
        for (int i = 0; i < row_elems; i++)
        {
            const float x01 = row_ptr[i] / 255.0f;
            int q = static_cast<int>(lroundf(x01 / kMidasInScale + kMidasInZeroPoint));
            if (q < 0)
                q = 0;
            if (q > 255)
                q = 255;
            dst[i] = static_cast<uint8_t>(q);
        }
        dst += row_elems;
    }

    if (interpreter->Invoke() != kTfLiteOk)
    {
        fprintf(stderr, "MidasModelHelper: Invoke() failed\n");
        return false;
    }

    const int64_t end_time = rc_nanos_monotonic_time();
    if (en_timing)
        total_inference_time += (end_time - start_time) / 1000000.0f;
    if (last_inference_time != nullptr)
        *last_inference_time = static_cast<double>(end_time - start_time) / 1e6;

    return true;
}

bool MidasModelHelper::postprocess(cv::Mat &output_image, double last_inference_time,
                                   void *input_params)
{
    if (input_params == nullptr)
    {
        fprintf(stderr, "MidasModelHelper: postprocess requires MidasFrameParams\n");
        return false;
    }

    MidasFrameParams *params = static_cast<MidasFrameParams *>(input_params);
    start_time = rc_nanos_monotonic_time();

    TfLiteTensor *output_tensor = interpreter->tensor(interpreter->outputs()[0]);
    cv::Mat depth_image(model_height, model_width, CV_32FC1);

    if (output_tensor->type == kTfLiteUInt8)
    {
        uint8_t *q_depth = TensorData<uint8_t>(output_tensor, 0);
        float *depth = reinterpret_cast<float *>(depth_image.data);
        for (int i = 0; i < model_height * model_width; i++)
            depth[i] = (static_cast<float>(q_depth[i]) - kMidasOutZeroPoint) * kMidasOutScale;
    }
    else if (output_tensor->type == kTfLiteFloat32)
    {
        float *depth = TensorData<float>(output_tensor, 0);
        memcpy(depth_image.data, depth, model_height * model_width * sizeof(float));
    }
    else
    {
        fprintf(stderr, "MidasModelHelper: unsupported output type %d\n",
                output_tensor->type);
        return false;
    }

    params->meta.height = model_height;
    params->meta.width = model_width;

#if MIDAS_PUBLISH_RAW
    (void)last_inference_time;  // only used by the FPS overlay in the colormap path
    // Single-channel raw depth: lossless and logged as PNG by voxl-logger.
    // depth = (q - zp) * scale  =>  q = depth / scale + zp  (exact for uint8 output).
    params->meta.size_bytes = params->meta.width * params->meta.height;
    params->meta.stride = params->meta.width;
    params->meta.format = IMAGE_FORMAT_RAW8;
    depth_image.convertTo(output_image, CV_8U, 1.0 / kMidasOutScale, kMidasOutZeroPoint);

    if (en_timing)
        total_postprocess_time += (rc_nanos_monotonic_time() - start_time) / 1000000.0f;
#else
    params->meta.size_bytes = params->meta.width * params->meta.height * 3;
    params->meta.stride = params->meta.width * 3;
    params->meta.format = IMAGE_FORMAT_RGB;

    double min_val = 0.0;
    double max_val = 0.0;
    cv::Mat depthmap_visual;
    cv::minMaxLoc(depth_image, &min_val, &max_val);
    depthmap_visual = 255 * (depth_image - min_val) / (max_val - min_val + 1e-6);
    depthmap_visual.convertTo(depthmap_visual, CV_8U);
    cv::applyColorMap(depthmap_visual, output_image, cv::COLORMAP_JET);

    if (en_timing)
        total_postprocess_time += (rc_nanos_monotonic_time() - start_time) / 1000000.0f;

    draw_fps(output_image, last_inference_time, cv::Point(0, 0), 0.5, 2,
             cv::Scalar(0, 0, 0), cv::Scalar(180, 180, 180), true);
#endif

    return true;
}

bool MidasModelHelper::worker(cv::Mat &output_image, double last_inference_time,
                              camera_image_metadata_t metadata, void *input_params)
{
    (void)input_params;

    MidasFrameParams params(metadata);

    if (!postprocess(output_image, last_inference_time, &params))
        return false;

    // Keep the source frame's capture timestamp (do NOT overwrite with publish
    // time) so downstream consumers can sync this depth to VIO / hires at the
    // capture instant rather than after the inference latency.
    pipe_server_write_camera_frame(IMAGE_CH, params.meta,
                                   reinterpret_cast<char *>(output_image.data));
    return true;
}
