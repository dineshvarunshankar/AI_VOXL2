#ifndef DEPTH_OUTPUT_H
#define DEPTH_OUTPUT_H

#include <cstddef> 
#include <modal_pipe_interfaces.h>
#include <opencv2/core.hpp>

void publish_float_image(int channel,
                         const camera_image_metadata_t &source_metadata,
                         const cv::Mat &image);

#endif
