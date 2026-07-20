#include "depth_utils/depth_preprocessor.h"

#include <cstdio>
#include <cstdlib>
#include <opencv2/imgproc.hpp>

namespace
{
constexpr const char *CONFIG_PATH =
    "/etc/voxl-tflite-server/undistort.yml";
}

DepthPreprocessor::DepthPreprocessor(const char *name)
    : _name(name)
{
}

DepthPreprocessor::~DepthPreprocessor()
{
    free(_map.L);
}

bool DepthPreprocessor::initialize(int input_width, int input_height,
                                   int model_width, int model_height)
{
    if (undistort_config_load(CONFIG_PATH, &_config) != 0)
        return false;

    if (_config.calib.width != input_width ||
        _config.calib.height != input_height)
    {
        fprintf(stderr, "%s: calibration is %dx%d, input is %dx%d\n",
                _name, _config.calib.width, _config.calib.height,
                input_width, input_height);
        return false;
    }

    double model_k[9]{};
    if (mcv_init_undistort_resize_map(
            &_config.calib, model_width, model_height,
            _config.fov, &_map, model_k) != 0)
        return false;

    fprintf(stderr,
            "%s: K_model=[%.3f 0 %.3f; 0 %.3f %.3f] "
            "publish_image=%d publish_disparity=%d\n",
            _name, model_k[0], model_k[2], model_k[4], model_k[5],
            _config.publish_image, _config.publish_disparity);
    _model_input.create(model_height, model_width, CV_8UC3);
    _ready = true;
    return true;
}

bool DepthPreprocessor::process(
    camera_image_metadata_t &meta, char *frame,
    int model_width, int model_height,
    std::shared_ptr<cv::Mat> preprocessed_image,
    std::shared_ptr<cv::Mat> output_image)
{
    if (!_ready &&
        !initialize(meta.width, meta.height, model_width, model_height))
        return false;

    switch (meta.format)
    {
    case IMAGE_FORMAT_STEREO_NV12:
    case IMAGE_FORMAT_NV12:
    {
        cv::Mat yuv(meta.height + meta.height / 2, meta.width, CV_8UC1, frame);
        cv::cvtColor(yuv, _rgb, cv::COLOR_YUV2RGB_NV12);
        break;
    }
    case IMAGE_FORMAT_STEREO_NV21:
    case IMAGE_FORMAT_NV21:
    {
        cv::Mat yuv(meta.height + meta.height / 2, meta.width, CV_8UC1, frame);
        cv::cvtColor(yuv, _rgb, cv::COLOR_YUV2RGB_NV21);
        break;
    }
    case IMAGE_FORMAT_YUV422:
    {
        cv::Mat yuv(meta.height, meta.width, CV_8UC2, frame);
        cv::cvtColor(yuv, _rgb, cv::COLOR_YUV2RGB_YUYV);
        break;
    }
    case IMAGE_FORMAT_STEREO_RAW8:
    case IMAGE_FORMAT_RAW8:
    {
        cv::Mat gray(meta.height, meta.width, CV_8UC1, frame);
        cv::cvtColor(gray, _rgb, cv::COLOR_GRAY2RGB);
        break;
    }
    default:
        fprintf(stderr, "%s: unsupported image format %d\n",
                _name, meta.format);
        return false;
    }

    mcv_resize_8uc3_image(_rgb.data, _model_input.data, &_map);
    *output_image = _rgb;
    *preprocessed_image = _model_input;
    meta.format = IMAGE_FORMAT_RGB;
    meta.size_bytes = meta.width * meta.height * 3;
    meta.stride = meta.width * 3;
    return true;
}
