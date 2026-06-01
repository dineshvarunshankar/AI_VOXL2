#include "model_helper/da3_model_helper.h"

#include <cstring>

#include "image_utils.h"
#include "tensor_data.h"

Da3ModelHelper::Da3ModelHelper(char *model_file, char *labels_file, DelegateOpt delegate_choice,
                                 bool _en_debug, bool _en_timing, NormalizationType _do_normalize)
    : ModelHelper(model_file, labels_file, delegate_choice, _en_debug, _en_timing, _do_normalize)
{
}

bool Da3ModelHelper::run_inference(cv::Mat &preprocessed_image, double *last_inference_time)
{
    start_time = rc_nanos_monotonic_time();

    TfLiteTensor *input_tensor = interpreter->tensor(interpreter->inputs()[0]);

    if (input_tensor->type != kTfLiteFloat32)
    {
        fprintf(stderr, "Da3ModelHelper: expected float32 input, got type %d\n",
                input_tensor->type);
        return false;
    }

    float *dst = TensorData<float>(input_tensor, 0);
    const int row_elems = model_width * model_channels;

    for (int row = 0; row < model_height; row++)
    {
        const uchar *row_ptr = preprocessed_image.ptr(row);
        for (int i = 0; i < row_elems; i++)
            dst[i] = row_ptr[i] / 255.0f;
        dst += row_elems;
    }

    if (interpreter->Invoke() != kTfLiteOk)
    {
        fprintf(stderr, "Da3ModelHelper: Invoke() failed\n");
        return false;
    }

    const int64_t end_time = rc_nanos_monotonic_time();
    if (en_timing)
        total_inference_time += (end_time - start_time) / 1000000.0f;
    if (last_inference_time != nullptr)
        *last_inference_time = static_cast<double>(end_time - start_time) / 1e6;

    return true;
}

bool Da3ModelHelper::postprocess(cv::Mat &output_image, double last_inference_time, void *input_params)
{
    if (input_params == nullptr)
        return false;

    Da3FrameParams *params = static_cast<Da3FrameParams *>(input_params);
    start_time = rc_nanos_monotonic_time();

    TfLiteTensor *output_tensor = interpreter->tensor(interpreter->outputs()[0]);

    if (output_tensor->type != kTfLiteFloat32)
    {
        fprintf(stderr, "Da3ModelHelper: expected float32 output, got type %d\n",
                output_tensor->type);
        return false;
    }

    cv::Mat depth_image(model_height, model_width, CV_32FC1);
    float *depth = TensorData<float>(output_tensor, 0);
    memcpy(depth_image.data, depth, model_height * model_width * sizeof(float));

    params->meta.height = model_height;
    params->meta.width = model_width;
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

    draw_fps(output_image, last_inference_time, cv::Point(0, 0), 0.5, 2, cv::Scalar(0, 0, 0),
             cv::Scalar(180, 180, 180), true);

    return true;
}

bool Da3ModelHelper::worker(cv::Mat &output_image, double last_inference_time,
                            camera_image_metadata_t metadata, void *input_params)
{
    (void)input_params;

    Da3FrameParams frame_params(metadata);

    if (!postprocess(output_image, last_inference_time, &frame_params))
        return false;

    frame_params.meta.timestamp_ns = rc_nanos_monotonic_time();
    pipe_server_write_camera_frame(IMAGE_CH, frame_params.meta,
                                   reinterpret_cast<char *>(output_image.data));
    return true;
}
