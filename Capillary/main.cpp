#include "Capillary.h"
//#include <QtWidgets/QApplication>

#include <iostream>
using std::cout;
using std::endl;

// ��¼����˼·���ȿ������������þֲ���ֵ���������ҵ�ROI
// ��ROI�����⣬����ͨ����

//���Դ���
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
	//����5����
	Mat structKel = getStructuringElement(MORPH_RECT, Size(10, 2));//���νṹԪ��
	Mat dilateImg;
	dilate(res, dilateImg, structKel);

	Mat erodeImg;
	Mat structKe2 = getStructuringElement(MORPH_RECT, Size(2, 10));//���νṹԪ��
	erode(dilateImg, erodeImg, structKe2);

	



	return 1;
 /*   QApplication a(argc, argv);
    Capillary w;
    w.show();
    return a.exec();*/
}
