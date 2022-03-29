#include "CapillaryAlg.h"

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
//#include <opencv2/imgproc/types_c.h>

using namespace cv;


//局部二值化
Mat dyn_threshold(Mat src, Mat dst, const int&& ValueDiffer)
{
	if (src.channels() == 3)
		cvtColor(src, src, cv::COLOR_BGR2GRAY);
	if (dst.channels() == 3)
		cvtColor(dst, dst, cv::COLOR_BGR2GRAY);

	Mat res = Mat::zeros(src.size(), CV_8UC1);
	for (size_t i = 0; i < src.rows; i++)
	{
		for (size_t j = 0; j < src.cols; j++)
		{
			int srcValue = src.at<uchar>(i, j);
			int dstValue = dst.at<uchar>(i, j);
			if (srcValue - dstValue < ValueDiffer)
			{
				res.at<uchar>(i, j) = 0;
			}
			else
			{
				res.at<uchar>(i, j) = 255;
			}

		}
	}

	return res;
}

Mat dyn_threshold_short(Mat src, Mat dst, const int&& ValueDiffer)
{
	if (src.channels() == 3)
		cvtColor(src, src, cv::COLOR_BGR2GRAY);
	if (dst.channels() == 3)
		cvtColor(dst, dst, cv::COLOR_BGR2GRAY);

	Mat res = Mat::zeros(src.size(), CV_16UC1);
	for (size_t i = 0; i < src.rows; i++)
	{
		for (size_t j = 0; j < src.cols; j++)
		{
			int srcValue = src.at<unsigned short>(i, j);
			int dstValue = dst.at<unsigned short>(i, j);
			if (srcValue - dstValue < ValueDiffer)
			{
				res.at<unsigned short>(i, j) = 0;
			}
			else
			{
				res.at<unsigned short>(i, j) = 255*256;
			}

		}
	}

	return res;
}