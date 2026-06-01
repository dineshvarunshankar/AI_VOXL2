#ifndef MIDAS_MODEL_HELPER_H
#define MIDAS_MODEL_HELPER_H

#include "model_helper/model_helper.h"

// Qualcomm AI Hub MiDaS w8a8 export.
static constexpr float kMidasInScale = 0.00487531116232276f;
static constexpr int kMidasInZeroPoint = 24;
static constexpr float kMidasOutScale = 6.514300346374512f;
static constexpr int kMidasOutZeroPoint = 0;

struct MidasFrameParams
{
    camera_image_metadata_t &meta;
    explicit MidasFrameParams(camera_image_metadata_t &meta_) : meta(meta_) {}
};

class MidasModelHelper : public ModelHelper
{
public:
    MidasModelHelper(char *model_file, char *labels_file, DelegateOpt delegate_choice,
                     bool _en_debug, bool _en_timing, NormalizationType _do_normalize);

    bool run_inference(cv::Mat &preprocessed_image, double *last_inference_time) override;
    bool postprocess(cv::Mat &output_image, double last_inference_time,
                     void *input_params) override;
    bool worker(cv::Mat &output_image, double last_inference_time,
                camera_image_metadata_t metadata, void *input_params) override;
};

#endif
