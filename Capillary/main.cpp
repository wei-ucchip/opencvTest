#include "Capillary.h"
//#include <QtWidgets/QApplication>

#include <iostream>
using std::cout;
using std::endl;

// 记录测试思路，先框出检测区域，利用局部二值化、膨胀找到ROI
// 对ROI区域检测，找联通区域？

//测试代码
#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "CapillaryAlg.h"
//using cv::Mat;
using namespace cv;


int main(int argc, char *argv[])
{
	Mat src = cv::imread("1.tiff", 0);
	
	Mat test;
	equalizeHist(src, test);
	GaussianBlur(test, test, Size(5, 5), 3, 3);
	GaussianBlur(test, test, Size(5, 5), 3, 3);
	Mat dst2 = Mat::zeros(test.size(), CV_8UC1);
	for (size_t i = 0; i < test.rows; i++)
	{
		for (size_t j = 0; j < test.cols; j++)
		{
			int srcValue = test.at<uchar>(i, j);
			if (srcValue < 20)
			{
				dst2.at<uchar>(i, j) = 255;
			}
			else if(srcValue >120)
			{
				dst2.at<uchar>(i, j) = 255;
			}
			else
			{
				dst2.at<uchar>(i, j) = srcValue;
			}

		}
	}

	//namedWindow("show");
	//imshow("show",src);
	//waitKey(0);
	Mat dst;
	blur(src, dst, Size(50, 50));
	Mat res = dyn_threshold(src, dst, 1);
	//Mat res = dyn_threshold_short(src, dst, 255);
	//膨胀5像素
	Mat structKel = getStructuringElement(MORPH_RECT, Size(10, 2));//矩形结构元素
	Mat dilateImg;
	dilate(res, dilateImg, structKel);

	Mat erodeImg;
	Mat structKe2 = getStructuringElement(MORPH_RECT, Size(2, 10));//矩形结构元素
	erode(dilateImg, erodeImg, structKe2);

	



	return 1;
 /*   QApplication a(argc, argv);
    Capillary w;
    w.show();
    return a.exec();*/
}
