#ifndef DEPTH_OUTPUT_H
#define DEPTH_OUTPUT_H

#include <cstddef>   // size_t — modal_pipe_interfaces.h/vio_data_t.h use it undeclared
#include <modal_pipe_interfaces.h>
#include <opencv2/core.hpp>

void publish_float_image(int channel,
                         const camera_image_metadata_t &source_metadata,
                         const cv::Mat &image);

#endif
