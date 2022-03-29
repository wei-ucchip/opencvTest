#ifndef __CAPILLARYALG_H_
#define __CAPILLARYALG_H_

#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>

using cv::Mat;


class CapillaryAlg
{

};

Mat dyn_threshold(Mat src, Mat dst, const int&& ValueDiffer = 40);
Mat dyn_threshold_short(Mat src, Mat dst, const int&& ValueDiffer);

#endif //__CAPILLARYALG_H_