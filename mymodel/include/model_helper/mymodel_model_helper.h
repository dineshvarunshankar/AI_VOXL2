#ifndef MYMODEL_MODEL_HELPER_H
#define MYMODEL_MODEL_HELPER_H

#include "model_helper/model_helper.h"

// Template notes:
// - Keep only the constants your model needs.
// - Choose the matching ModelCategory when wiring the helper into main.cpp.

// uint8/int8 — uncomment and fill:
// static constexpr float kMyModelInScale = 0.0f;
// static constexpr int   kMyModelInZeroPoint = 0;
// static constexpr float kMyModelOutScale = 0.0f;
// static constexpr int   kMyModelOutZeroPoint = 0;

// float32 — use TensorData<float> in .cpp; mean/std only if your model needs them:
// static constexpr float kMyModelMean[3] = {0.485f, 0.456f, 0.406f};
// static constexpr float kMyModelStd[3]  = {0.229f, 0.224f, 0.225f};

struct MyModelFrameParams
{
    camera_image_metadata_t &meta;
    explicit MyModelFrameParams(camera_image_metadata_t &meta_) : meta(meta_) {}
};

class MyModelModelHelper : public ModelHelper
{
public:
    MyModelModelHelper(char *model_file, char *labels_file, DelegateOpt delegate_choice,
                       bool _en_debug, bool _en_timing, NormalizationType _do_normalize);

    bool run_inference(cv::Mat &preprocessed_image, double *last_inference_time) override;
    bool postprocess(cv::Mat &output_image, double last_inference_time,
                     void *input_params) override;
    bool worker(cv::Mat &output_image, double last_inference_time,
                camera_image_metadata_t metadata, void *input_params) override;
};

#endif
