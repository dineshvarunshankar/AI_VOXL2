#ifndef UNDISTORT_MAP_H
#define UNDISTORT_MAP_H

#include <opencv2/core.hpp>

extern "C" {
#include "resize.h"
}

typedef enum { CAM_PINHOLE, CAM_FISHEYE } cam_model_t;
typedef enum { FOV_CROP, FOV_STRETCH } fov_mode_t;

typedef struct {
    double fx, fy, cx, cy;
    double d[5];
    int width, height;
    cam_model_t model;
} camera_calib_t;

typedef struct {
    fov_mode_t fov;
    int publish_image;      // 0|1  JET visualization on IMAGE_CH
    int publish_disparity;  // 0|1  FLOAT32 map on DISPARITY_CH
    camera_calib_t calib;
} undistort_config_t;

int undistort_config_load(const char* path, undistort_config_t* cfg);

int mcv_init_undistort_resize_map(const camera_calib_t* calib, int w_out, int h_out,
                                  fov_mode_t fov, undistort_map_t* map,
                                  double K_model[9]);

#endif
