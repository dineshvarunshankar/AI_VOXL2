#include "depth_utils/undistort_map.h"

#include <opencv2/calib3d.hpp>
#include <opencv2/core/persistence.hpp>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>

int undistort_config_load(const char* path, undistort_config_t* cfg)
{
    cv::FileStorage fs(path, cv::FileStorage::READ);
    if (!fs.isOpened()) {
        fprintf(stderr, "undistort: cannot open %s\n", path);
        return -1;
    }

    cfg->publish_image = (int)fs["publish_image"];
    cfg->publish_disparity = (int)fs["publish_disparity"];
    if (cfg->publish_image != 0 && cfg->publish_image != 1) {
        fprintf(stderr, "undistort: publish_image must be 0 or 1\n");
        return -1;
    }
    if (cfg->publish_disparity != 0 && cfg->publish_disparity != 1) {
        fprintf(stderr, "undistort: publish_disparity must be 0 or 1\n");
        return -1;
    }
    if (!cfg->publish_image && !cfg->publish_disparity) {
        fprintf(stderr, "undistort: enable publish_image and/or publish_disparity\n");
        return -1;
    }

    const std::string fov = (std::string)fs["fov"];
    if (fov == "crop")         cfg->fov = FOV_CROP;
    else if (fov == "stretch") cfg->fov = FOV_STRETCH;
    else {
        fprintf(stderr, "undistort: fov must be crop|stretch, got '%s'\n",
                fov.c_str());
        return -1;
    }

    cv::FileNode c = fs["camera"];
    if (c.empty()) {
        fprintf(stderr, "undistort: %s has no 'camera' block\n", path);
        return -1;
    }

    const std::string model = (std::string)c["model"];
    if (model == "fisheye")      cfg->calib.model = CAM_FISHEYE;
    else if (model == "pinhole") cfg->calib.model = CAM_PINHOLE;
    else {
        fprintf(stderr, "undistort: camera.model must be fisheye|pinhole, got '%s'\n",
                model.c_str());
        return -1;
    }

    cfg->calib.width  = (int)c["width"];
    cfg->calib.height = (int)c["height"];
    cfg->calib.fx = (double)c["fx"];
    cfg->calib.fy = (double)c["fy"];
    cfg->calib.cx = (double)c["cx"];
    cfg->calib.cy = (double)c["cy"];

    cv::FileNode d = c["distortion"];
    const bool fisheye = cfg->calib.model == CAM_FISHEYE;
    if (fisheye ? d.size() != 4 : (d.size() != 4 && d.size() != 5)) {
        fprintf(stderr, "undistort: %s needs %s coefficients, got %zu\n",
                fisheye ? "fisheye" : "pinhole",
                fisheye ? "4" : "4 or 5", d.size());
        return -1;
    }
    cfg->calib.d[4] = 0.0;
    const int need = fisheye ? 4 : (int)d.size();
    for (int i = 0; i < need; i++) cfg->calib.d[i] = (double)d[i];

    if (!std::isfinite(cfg->calib.fx) || !std::isfinite(cfg->calib.fy) ||
        !std::isfinite(cfg->calib.cx) || !std::isfinite(cfg->calib.cy) ||
        cfg->calib.fx <= 0 || cfg->calib.fy <= 0 ||
        cfg->calib.width <= 0 || cfg->calib.height <= 0) {
        fprintf(stderr, "undistort: %s has implausible intrinsics\n", path);
        return -1;
    }
    return 0;
}

static cv::Mat calib_K(const camera_calib_t* c)
{
    return (cv::Mat_<double>(3, 3) << c->fx, 0, c->cx, 0, c->fy, c->cy, 0, 0, 1);
}

static cv::Mat calib_D(const camera_calib_t* c)
{
    if (c->model == CAM_FISHEYE)
        return (cv::Mat_<double>(4, 1) << c->d[0], c->d[1], c->d[2], c->d[3]);
    return (cv::Mat_<double>(5, 1) << c->d[0], c->d[1], c->d[2], c->d[3], c->d[4]);
}

static void source_fov_tan(const camera_calib_t* c, double* x_max, double* y_max)
{
    const double w = c->width, h = c->height;
    std::vector<cv::Point2d> edges = {
        {0.0, h / 2}, {w - 1.0, h / 2}, {w / 2, 0.0}, {w / 2, h - 1.0}};
    std::vector<cv::Point2d> n;

    if (c->model == CAM_FISHEYE)
        cv::fisheye::undistortPoints(edges, n, calib_K(c), calib_D(c));
    else
        cv::undistortPoints(edges, n, calib_K(c), calib_D(c));

    *x_max = std::max(std::abs(n[0].x), std::abs(n[1].x));
    *y_max = std::max(std::abs(n[2].y), std::abs(n[3].y));
}

static cv::Mat build_k_model(const camera_calib_t* c, int w_out, int h_out, fov_mode_t fov)
{
    double x_max, y_max;
    source_fov_tan(c, &x_max, &y_max);

    const double hw = w_out / 2.0, hh = h_out / 2.0;
    double fx, fy;

    if (fov == FOV_STRETCH) {
        fx = hw / x_max;
        fy = hh / y_max;
    } else {
        const double f = std::max(hw / x_max, hh / y_max);
        fx = fy = f;
    }
    return (cv::Mat_<double>(3, 3) << fx, 0, hw, 0, fy, hh, 0, 0, 1);
}

static int pack_lut(const cv::Mat& mx, const cv::Mat& my,
                    int w_in, int h_in, int w_out, int h_out,
                    undistort_map_t* map)
{
    map->w_in = w_in;
    map->h_in = h_in;
    map->w_out = w_out;
    map->h_out = h_out;
    map->L = (bilinear_lookup_t*)malloc(w_out * h_out * sizeof(bilinear_lookup_t));
    if (map->L == NULL) return -1;

    for (int v = 0; v < h_out; v++) {
        for (int u = 0; u < w_out; u++) {
            const int pix = v * w_out + u;
            const float sx = mx.at<float>(v, u);
            const float sy = my.at<float>(v, u);
            const int x1 = (int)std::floor(sx);
            const int y1 = (int)std::floor(sy);

            if (x1 < 0 || y1 < 0 || x1 + 1 > w_in - 1 || y1 + 1 > h_in - 1) {
                map->L[pix].I[0] = -1;
                map->L[pix].I[1] = -1;
                continue;
            }

            const float wx = sx - x1;
            const float wy = sy - y1;
            map->L[pix].I[0] = (int16_t)x1;
            map->L[pix].I[1] = (int16_t)y1;
            map->L[pix].F[0] = (uint8_t)((1 - wx) * (1 - wy) * 256);
            map->L[pix].F[1] = (uint8_t)(wx * (1 - wy) * 256);
            map->L[pix].F[2] = (uint8_t)((1 - wx) * wy * 256);
            map->L[pix].F[3] = (uint8_t)(wx * wy * 256);
        }
    }
    return 0;
}

int mcv_init_undistort_resize_map(const camera_calib_t* calib, int w_out, int h_out,
                                  fov_mode_t fov, undistort_map_t* map,
                                  double K_model[9])
{
    const cv::Mat K = calib_K(calib);
    const cv::Mat D = calib_D(calib);
    const cv::Mat P = build_k_model(calib, w_out, h_out, fov);

    cv::Mat mx, my;
    if (calib->model == CAM_FISHEYE)
        cv::fisheye::initUndistortRectifyMap(K, D, cv::Mat::eye(3, 3, CV_64F), P,
                                             cv::Size(w_out, h_out), CV_32FC1, mx, my);
    else
        cv::initUndistortRectifyMap(K, D, cv::Mat(), P, cv::Size(w_out, h_out),
                                    CV_32FC1, mx, my);

    for (int i = 0; i < 9; i++) K_model[i] = P.at<double>(i / 3, i % 3);

    return pack_lut(
        mx, my, calib->width, calib->height, w_out, h_out, map);
}
