#ifndef DEPTH_PREPROCESSOR_H
#define DEPTH_PREPROCESSOR_H

#include "depth_utils/undistort_map.h"

#include <memory>
#include <opencv2/core.hpp>
#include <modal_pipe_interfaces.h>

class DepthPreprocessor
{
public:
    explicit DepthPreprocessor(const char *name);
    ~DepthPreprocessor();

    bool process(camera_image_metadata_t &meta, char *frame,
                 int model_width, int model_height,
                 std::shared_ptr<cv::Mat> preprocessed_image,
                 std::shared_ptr<cv::Mat> output_image);

    bool publish_image() const { return _config.publish_image != 0; }
    bool publish_disparity() const { return _config.publish_disparity != 0; }

private:
    bool initialize(int input_width, int input_height,
                    int model_width, int model_height);

    const char *_name;
    undistort_config_t _config{};
    undistort_map_t _map{};
    bool _ready{false};
    cv::Mat _rgb;
    cv::Mat _model_input;
};

#endif
