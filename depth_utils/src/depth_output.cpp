#include "depth_utils/depth_output.h"

#include <cstdio>
#include <modal_pipe.h>

void publish_float_image(int channel,
                         const camera_image_metadata_t &source_metadata,
                         const cv::Mat &image)
{
    if (image.type() != CV_32FC1 || !image.isContinuous())
    {
        fprintf(stderr, "depth output must be contiguous CV_32FC1\n");
        return;
    }

    camera_image_metadata_t metadata = source_metadata;
    metadata.width = image.cols;
    metadata.height = image.rows;
    metadata.format = IMAGE_FORMAT_FLOAT32;
    metadata.stride = image.cols * sizeof(float);
    metadata.size_bytes = metadata.stride * image.rows;
    pipe_server_write_camera_frame(
        channel, metadata, reinterpret_cast<char *>(image.data));
}
