#ifndef DA3_MODEL_HELPER_H
#define DA3_MODEL_HELPER_H

#include "model_helper/model_helper.h"

// depth_anything_v3.tflite — float32, 518x518x3 in, depth out (NHWC)

struct Da3FrameParams
{
    camera_image_metadata_t &meta;
    explicit Da3FrameParams(camera_image_metadata_t &meta_) : meta(meta_) {}
};

class Da3ModelHelper : public ModelHelper
{
public:
    Da3ModelHelper(char *model_file, char *labels_file, DelegateOpt delegate_choice,
                   bool _en_debug, bool _en_timing, NormalizationType _do_normalize);

    bool run_inference(cv::Mat &preprocessed_image, double *last_inference_time) override;
    bool postprocess(cv::Mat &output_image, double last_inference_time,
                     void *input_params) override;
    bool worker(cv::Mat &output_image, double last_inference_time,
                camera_image_metadata_t metadata, void *input_params) override;
};

#endif
