#include "model_helper/mymodel_model_helper.h"

#include "image_utils.h"
#include "tensor_data.h"


// 1. Constructor


MyModelModelHelper::MyModelModelHelper(char *model_file, char *labels_file,
                                       DelegateOpt delegate_choice, bool _en_debug,
                                       bool _en_timing, NormalizationType _do_normalize)
    : ModelHelper(model_file, labels_file, delegate_choice, _en_debug, _en_timing,
                  _do_normalize)
{
}


// 2. run_inference


bool MyModelModelHelper::run_inference(cv::Mat &preprocessed_image,
                                       double *last_inference_time)
{
    start_time = rc_nanos_monotonic_time();

    TfLiteTensor *input_tensor = interpreter->tensor(interpreter->inputs()[0]);

    // TODO: fill input_tensor from preprocessed_image.
    // Use model_height, model_width, and model_channels for image models.
    // For quantized models, apply scale/zero-point here before Invoke().
    (void)preprocessed_image;
    (void)input_tensor;

    if (interpreter->Invoke() != kTfLiteOk)
    {
        fprintf(stderr, "MyModelModelHelper: Invoke() failed\n");
        return false;
    }

    const int64_t end_time = rc_nanos_monotonic_time();
    if (en_timing)
        total_inference_time += (end_time - start_time) / 1000000.0f;
    if (last_inference_time != nullptr)
        *last_inference_time = static_cast<double>(end_time - start_time) / 1e6;

    return true;
}


// 3. postprocess


bool MyModelModelHelper::postprocess(cv::Mat &output_image, double last_inference_time,
                                     void *input_params)
{
    if (input_params == nullptr)
        return false;

    MyModelFrameParams *params = static_cast<MyModelFrameParams *>(input_params);
    start_time = rc_nanos_monotonic_time();

    TfLiteTensor *output_tensor = interpreter->tensor(interpreter->outputs()[0]);

    // TODO: read output_tensor and build output_image or another published output.
    // Examples: depth colormap, segmentation mask, detections, logits, or embeddings.
    (void)output_tensor;

    params->meta.height = model_height;
    params->meta.width = model_width;
    params->meta.size_bytes = params->meta.width * params->meta.height * 3;
    params->meta.stride = params->meta.width * 3;
    params->meta.format = IMAGE_FORMAT_RGB;

    if (en_timing)
        total_postprocess_time += (rc_nanos_monotonic_time() - start_time) / 1000000.0f;

    draw_fps(output_image, last_inference_time, cv::Point(0, 0), 0.5, 2, cv::Scalar(0, 0, 0),
             cv::Scalar(180, 180, 180), true);

    return true;
}


// 4. worker


bool MyModelModelHelper::worker(cv::Mat &output_image, double last_inference_time,
                                camera_image_metadata_t metadata, void *input_params)
{
    (void)input_params;

    MyModelFrameParams frame_params(metadata);

    if (!postprocess(output_image, last_inference_time, &frame_params))
        return false;

    frame_params.meta.timestamp_ns = rc_nanos_monotonic_time();
    // TODO: For non-image outputs, replace IMAGE_CH publishing with the pipe
    // used by your model type (detections, classifications, embeddings, etc.).
    pipe_server_write_camera_frame(IMAGE_CH, frame_params.meta,
                                   reinterpret_cast<char *>(output_image.data));
    return true;
}
