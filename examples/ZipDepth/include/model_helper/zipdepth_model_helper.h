#ifndef ZIPDEPTH_MODEL_HELPER_H
#define ZIPDEPTH_MODEL_HELPER_H

#include "depth_utils/depth_preprocessor.h"
#include "model_helper/model_helper.h"

// ZipDepth float32 export: 384x384x3 in, inverse-depth out (NHWC).

struct ZipDepthFrameParams
{
    camera_image_metadata_t &meta;
    explicit ZipDepthFrameParams(camera_image_metadata_t &meta_) : meta(meta_) {}
};

class ZipDepthModelHelper : public ModelHelper
{
public:
    ZipDepthModelHelper(char *model_file, char *labels_file, DelegateOpt delegate_choice,
                        bool _en_debug, bool _en_timing, NormalizationType _do_normalize);

    bool preprocess(camera_image_metadata_t &meta, char *frame,
                    std::shared_ptr<cv::Mat> preprocessed_image,
                    std::shared_ptr<cv::Mat> output_image) override;
    bool run_inference(cv::Mat &preprocessed_image, double *last_inference_time) override;
    bool postprocess(cv::Mat &output_image, double last_inference_time,
                     void *input_params) override;
    bool worker(cv::Mat &output_image, double last_inference_time,
                camera_image_metadata_t metadata, void *input_params) override;

private:
    DepthPreprocessor preprocessor{"ZipDepth"};
    cv::Mat disparity_image;
};

#endif
