#include "imgsys.h"

#define Orange Scalar(0, 128, 255)
#define Blue Scalar(255, 0, 0)
#define Red Scalar(0, 0, 255)
#define Green Scalar(0, 255, 0)
#define Yellow Scalar(255, 255, 0)
#define White Scalar(255, 255, 255)


ImgSys theApp;

Qt::CaseSensitivity cs = Qt::CaseInsensitive;//不区分大小写变量

MatManager *pnewMatManager;
bool savepushFlag=false;
QString newSN="";
vector<QString> log_msg;
QString CurrentTime="", LastTime="";

sift_Struct m_siftstruct[10];
double mate_ratio=0.0;
int bDebug = 0;
int ImgenhanceFlag = 0;
int SpecialAngle = -1;//0-宽=高，1-条码宽>高，2-条码宽<高
QMutex g_ZxingMute;

typedef string(*hDatamatrixdecode)(Mat &src, vector <string> &code, int timeout, int MaxCode);
hDatamatrixdecode mDatamatrixdecode = nullptr;

#include <QTextCodec>
void debuglog(QString str)
{
    //cout<<"imgsys debuglog bDebug = "<<bDebug<<endl;
    QString sstr = "imgsys debuglog : "+str;
    OutputDebugStringA(sstr.toStdString().c_str());

    if(!bDebug)
        return;

    //_wsetlocale(0, L"chs");//务必加上,否则生成的文本是问号字符串(fwprintf对中文不支持)
    setlocale(0, "chs");//务必加上否则生成的文本是问号字符串
    static QMutex mutex;
    mutex.lock();

    //QString context_info = QString("File:(%1) Line:(%2)").arg(QString(context.file)).arg(context.line);
    QString current_date_time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz dddd");
    QString current_date = QString("(%1)").arg(current_date_time);
    QString message = QString("%1 | %2").arg(current_date).arg(str);

    QFile file("ImgSys_Debug.Log");
    file.open(QIODevice::WriteOnly | QIODevice::Append);
    QTextStream text_stream(&file);
    text_stream << message << "\r\n";
    file.flush();
    file.close();

    mutex.unlock();
}

void writePstFuncLog(QString str, int index,DLLFunc *pstFunc)
{
    if (pstFunc != nullptr && pstFunc->mDLLLogCatfunc != nullptr)
    {
        //cout<<"pstFunc true."<<endl;
        debuglog(str);
        if(!bDebug)
            return;
        switch (index)
        {
        case 0:
        {
            pstFunc->mDLLLogCatfunc(str.toStdString(),eLog_Debug);
            break;
        }
        case 1:
        {
            pstFunc->mDLLLogCatfunc(str.toStdString(),eLog_Normal);
            break;
        }
        case 2:
        {
            pstFunc->mDLLLogCatfunc(str.toStdString(),eLog_Warning);
            break;
        }
        case 3:
        {
            pstFunc->mDLLLogCatfunc(str.toStdString(),eLog_Error);
            break;
        }
        case 4:
        {
            pstFunc->mDLLLogCatfunc(str.toStdString(),eLog_Fatal);
            break;
        }
        }
    }
    else
    {
        //cout<<"pstFunc false."<<endl;
        debuglog(str);
    }
    return;
}

ImgSys::ImgSys()
{
    QTextCodec *codec = QTextCodec::codecForName("utf8");//显示中文
    QTextCodec::setCodecForLocale(codec);
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));

    cout<<"ImgSys!"<<endl;
    //OutputDebugStringA("");
    QLibrary dtmxhandler("DatamatrixDecode.dll");
    if (dtmxhandler.load())
    {
        cout<<"DatamatrixDecode.dll load OK.."<<endl;
        mDatamatrixdecode = (hDatamatrixdecode)dtmxhandler.resolve("Datamatrixdecode");
    }
    else
    {
        DWORD error = GetLastError();
        QString err = ("");
        err = QString("imgsys loadLibrary DatamatrixDecode.dll fail,error code = %1..").arg(error);
        cout<<err.toStdString()<<endl;

        debuglog(err);
        //writePstFuncLog(err,eLog_Fatal, pFunc);
    }

}

ImgSys::~ImgSys()
{
    cout<<"~ImgSys!"<<endl;
}

typedef   struct _QDebugEx
{
   _QDebugEx(DLLFunc *vifunc):pFunc(vifunc){}
   explicit _QDebugEx(){}
   _QDebugEx operator<<(const string& str);
   _QDebugEx operator<<(const int& instr);
   DLLFunc *pFunc;
}QDebugEx;

inline _QDebugEx  _QDebugEx::operator<<(const string& str)
{
    if(pFunc != nullptr)
    {
        pFunc->mDLLLogCatfunc(str,eLog_Normal);
    }
    else {
        //utf-8 需要转换
        QString Msg = QString::fromStdString(str);
        QByteArray temp = Msg.toLocal8Bit();
        std::cout<<temp.data()<<endl;
        OutputDebugStringA(temp.data());
    }

    return *this;

}

inline _QDebugEx  _QDebugEx::operator<<(const int& instr)
{
    string str = to_string(instr);
    if(pFunc != nullptr)
    {
        pFunc->mDLLLogCatfunc(str,eLog_Normal);
    }
    else {
        //utf-8 需要转换
        QString Msg = QString::fromStdString(str);
        QByteArray temp = Msg.toLocal8Bit();
        std::cout<<temp.data()<<endl;
        OutputDebugStringA(temp.data());
    }

     return *this;
}


std::wstring s2ws(const std::string& s)
{
    int len;
    int slength = (int)s.length() + 1;
    len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
    wchar_t* buf = new wchar_t[len];
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
    std::wstring r(buf);
    delete[] buf;
    return r;
}
// wchar_t to string
void Wchar_tToString(std::string& szDst, wchar_t*wchar)
{
    wchar_t * wText = wchar;
    DWORD dwNum = WideCharToMultiByte(CP_OEMCP,NULL,wText,-1,NULL,0,NULL,FALSE);//WideCharToMultiByte的运用
    char *psText;  // psText为char*的临时数组，作为赋值给std::string的中间变量
    psText = new char[dwNum];
    WideCharToMultiByte (CP_OEMCP,NULL,wText,-1,psText,dwNum,NULL,FALSE);//WideCharToMultiByte的再次运用
    szDst = psText;// std::string赋值
    delete []psText;// psText的清除
}
// Mat To QImage
QImage MatToQImage(const cv::Mat/*&*/ mat)
{

    // 8-bits unsigned, NO. OF CHANNELS = 1
    if(mat.type() == CV_8UC1)
    {
        QImage image(mat.cols, mat.rows, QImage::Format_Indexed8);
        // Set the color table (used to translate colour indexes to qRgb values)
        image.setColorCount(256);
        for(int i = 0; i < 256; i++)
        {
            image.setColor(i, qRgb(i, i, i));
        }
        // Copy input Mat
        uchar *pSrc = mat.data;
        for(int row = 0; row < mat.rows; row ++)
        {
            uchar *pDest = image.scanLine(row);
            memcpy(pDest, pSrc, mat.cols);
            pSrc += mat.step;
        }
        return image;
    }
    // 8-bits unsigned, NO. OF CHANNELS = 3
    else if(mat.type() == CV_8UC3)
    {
        // Copy input Mat
        const uchar *pSrc = (const uchar*)mat.data;
        // Create QImage with same dimensions as input Mat
        QImage image(pSrc, mat.cols, mat.rows, mat.step, QImage::Format_RGB888);
        return image.rgbSwapped();
    }
    else if(mat.type() == CV_8UC4)
    {

        // Copy input Mat
        const uchar *pSrc = (const uchar*)mat.data;
        // Create QImage with same dimensions as input Mat
        QImage image(pSrc, mat.cols, mat.rows, mat.step, QImage::Format_ARGB32);
        return image.copy();
    }
    else
    {

        return QImage();
    }
}
// QImage To Mat
cv::Mat QImageToMat(QImage image)
{
    cv::Mat mat;
    switch (image.format())
    {
    case QImage::Format_ARGB32:
    case QImage::Format_RGB32:
    case QImage::Format_ARGB32_Premultiplied:
        mat = cv::Mat(image.height(), image.width(), CV_8UC4, (void*)image.constBits(), image.bytesPerLine());
        break;
    case QImage::Format_RGB888:
        mat = cv::Mat(image.height(), image.width(), CV_8UC3, (void*)image.constBits(), image.bytesPerLine());
        cv::cvtColor(mat, mat, cv::COLOR_BGR2RGB);
        break;	case QImage::Format_Indexed8:
        mat = cv::Mat(image.height(), image.width(), CV_8UC1, (void*)image.constBits(), image.bytesPerLine());
        break;
    }
    return mat;
}

wchar_t* Utf_8ToUnicode(const char* szU8)
{
    int wcsLen = ::MultiByteToWideChar(CP_UTF8, NULL, szU8, strlen(szU8), NULL, 0);
    wchar_t* wszString = new wchar_t[wcsLen + 1];
    ::MultiByteToWideChar(CP_UTF8, NULL, szU8, strlen(szU8), wszString, wcsLen);
    wszString[wcsLen] = '\0';
    return wszString;
}

char* UnicodeToAnsi(const wchar_t* szStr)
{
    int nLen = WideCharToMultiByte(CP_ACP, 0, szStr, -1, NULL, 0, NULL, NULL);
    if (nLen == 0)
    {
        return NULL;
    }
    char* pResult = new char[nLen];
    WideCharToMultiByte(CP_ACP, 0, szStr, -1, pResult, nLen, NULL, NULL);
    return pResult;
}

//移动到另外一个图中
int MoveToOtherMat(cv::Mat &BigPic, cv::Mat SmallPic, cv::Rect SrcRect)
{
    if(SrcRect.x + SrcRect.width > BigPic.cols || SrcRect.y + SrcRect.height > BigPic.rows)
    {
        return -1;
    }
    cv::Mat tmp = cv::Mat(BigPic, cv::Rect(SrcRect));
    SmallPic.copyTo(tmp);
    return 0;
}



namespace sdt
{
    //被迫增加一个全局参数
    static std::string StrDebugMsg = "";
    typedef enum _Fun_Return {
            Res_OK,
            Res_Fail,
            Res_Error
        }Fun_Return;

        typedef struct _RegionValue
        {
            unsigned char down;//低值
            unsigned char up;//高值
            _RegionValue():down(0),up(255)
            {
            }
            /*_RegionValue(const int &down1,const int &up1 ) :down(down1), up(up1)
            {
            }*/
            _RegionValue(unsigned char down1, unsigned char up1) :down(down1), up(up1)
            {
            }
        }RegionValue;


        typedef struct _HSV_region
        {
            RegionValue Hregion;
            RegionValue Sregion;
            RegionValue Vregion;
            _HSV_region():Hregion(_RegionValue()), Sregion(_RegionValue()), Vregion(_RegionValue())
            {

            }
             _HSV_region(RegionValue value1, RegionValue value2, RegionValue value3):Hregion(value1), Sregion(value2), Vregion(value3)
            {

            }
             /*HSV_region(const RegionValue &value1, const RegionValue &value2, const RegionValue & value3) :Hregion(value1), Sregion(value2), Vregion(value3)
            {

            }*/
        }HSV_region;


    /******************************算子***************************************/
    //随机生成RGB颜色
    void randRGBColor(int &R, int &G, int &B)
    {
        B = theRNG().uniform(0, 255);
        G = theRNG().uniform(0, 255);
        R = theRNG().uniform(0, 255);
    }

    //绘制轮廓函数
    void drawapp(Mat result, Mat img2)
    {
        int r, g, b;
        randRGBColor(r, g, b);

        for (int i = 0; i < result.rows; i++)
        {
            //最后一个坐标点与第一个坐标点连接
            if (i == result.rows - 1)
            {
                Vec2i point1 = result.at<Vec2i>(i);
                Vec2i point2 = result.at<Vec2i>(0);
                line(img2, point1, point2, Scalar(b, g, r), 2, 8, 0);
                break;
            }
            Vec2i point1 = result.at<Vec2i>(i);
            Vec2i point2 = result.at<Vec2i>(i + 1);
            line(img2, point1, point2, Scalar(b, g, r), 2, 8, 0);
        }

    }
    //resize
    void ReSizeImg(Mat &image, double scale = 0.5)
    {
        Size dsize = Size(image.cols*scale, image.rows*scale);
        resize(image.clone(), image, dsize);
    }
    //移动图像
    void MoveImg(Mat &src, int x, int y, Size dsize)//两张图大小不一样？
    {
        //定义平移矩阵
        cv::Mat t_mat = cv::Mat::zeros(2, 3, CV_32FC1);//[1,0,x;0,1,y]

        t_mat.at<float>(0, 0) = 1;
        t_mat.at<float>(0, 1) = 0;
        t_mat.at<float>(0, 2) = x; //水平平移量
        t_mat.at<float>(1, 0) = 0;
        t_mat.at<float>(1, 1) = 1;
        t_mat.at<float>(1, 2) = y; //竖直平移量

        warpAffine(src, src, t_mat, dsize);

    }
    //局部二值化
    Mat dyn_threshold(Mat src, Mat dst, const int& ValueDiffer = 40)
    {
        if (src.channels() == 3)
            cvtColor(src, src, COLOR_BGR2GRAY);
        if (dst.channels() == 3)
            cvtColor(dst, dst, COLOR_BGR2GRAY);

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

    int MaxDifferValue(uchar value1, uchar value2)
    {
        return abs(value1-value2);
    }
    //自定义灰度图变化
    int GetMatGray(Mat &src, Mat& dst, int B = 1, int G = 1, int R = 8)
    {
        if (src.channels() != 3)
            return Res_Fail;

        Mat dest[3];
        split(src, dest);
        //debug
        Mat img1 = dest[0];
        Mat img2 = dest[1];
        Mat img3 = dest[2];
        //遍历像素，将临域差异不大的点记录下来，重新画一张，即可记录下轮廓
        Mat res = Mat::zeros(dest[0].size(), CV_8UC1);
        for (int i = 0; i < dest[0].rows; i++)
        {
            for (int j = 0; j < dest[0].cols; j++)
            {
                //res.at<uchar>(i, j) = dest[0].at<uchar>(i, j) * 1 / 10 + dest[1].at<uchar>(i, j) * 1 / 10 + dest[2].at<uchar>(i, j) * 8 / 10; //1:1:8 权重
                res.at<uchar>(i, j) = dest[2].at<uchar>(i, j); //
            }
        }
        dst = res.clone();
        return Res_OK;
    }
    //HSV提取器件
    void _HsvSpliteMat(Mat &src, Mat &hsv,const RegionValue& Hregion, const RegionValue& Sregion, const RegionValue& Vregion)
    {
        CV_Assert(src.data != nullptr);
        CV_Assert(src.channels() == 3);//必须为3通道

        //HSV分离
        //Mat hsv;
        cvtColor(src.clone(), hsv, cv::COLOR_BGR2HSV);
        int Hsum = 0; int Ssum = 0; int Vsum = 0;
        for (int i = 0; i < hsv.rows; i++)
        {
            for (int j = 0; j < hsv.cols; j++)
            {
                Hsum = hsv.at<cv::Vec3b>(i, j)[0];
                Ssum = hsv.at<cv::Vec3b>(i, j)[1];
                Vsum = hsv.at<cv::Vec3b>(i, j)[2];
                if ((Hregion.up != Hregion.down) && (Hsum > Hregion.up || Hsum < Hregion.down))
                {
                    hsv.at<Vec3b>(i, j) = { 0,0,0 };
                    continue;
                }
                if (Sregion.down != Sregion.up && (Ssum > Sregion.up || Ssum < Sregion.down ))
                {
                    hsv.at<Vec3b>(i, j) = { 0,0,0 };
                    continue;
                }
                if (Vregion.down != Vregion.up && (Vsum > Vregion.up || Vsum < Vregion.down))
                {
                    hsv.at<Vec3b>(i, j) = { 0,0,0 };
                    continue;
                }

                hsv.at<Vec3b>(i, j) = { 0,0,255 };
            }
        }

        //
        cvtColor(hsv, hsv, COLOR_HSV2BGR);
    }
    //获取HSV平均值
    int GetImg_HSV_Average_Value(Mat &Roi_Img, int &Hnum, int &Snum, int &Vnum)
    {
        if (Roi_Img.channels() != 3)
        {
            return Res_Fail;
        }

        //在比较颜色
        cvtColor(Roi_Img, Roi_Img, COLOR_BGR2HSV);
        for (int i = 0; i < Roi_Img.rows; i++)
        {
            for (int j = 0; j < Roi_Img.cols; j++)
            {
                Hnum += Roi_Img.at<Vec3b>(i, j)[0];
                Snum += Roi_Img.at<Vec3b>(i, j)[1];
                Vnum += Roi_Img.at<Vec3b>(i, j)[2];
            }
        }

        Hnum = Hnum / Roi_Img.rows / Roi_Img.cols;
        Snum = Snum / Roi_Img.rows / Roi_Img.cols;
        Vnum = Vnum / Roi_Img.rows / Roi_Img.cols;

        return Res_OK;
    }


    void SaveDebugImg(string Path, Mat &img)
    {
        if (QDir("Debug").exists())
        {
            imwrite("./Debug/CN2_" + Path, img);
        }

    }

    void ShowDebugMsg(DLLFunc *pFunc, string &&str)
    {
//#define _VIFUNC 0
//#if _VIFUNC
        if (pFunc != nullptr)
            pFunc->mDLLLogCatfunc(str, eLog_Normal);
        else
        {
            //QString qMsg = QString::fromStdString(str);
            //QByteArray temparry = qMsg.toLocal8Bit();
            //cout << temparry.data() << endl;
            cout << str.data() << endl;
        }

        StrDebugMsg += str + "$$";

        //cout<<str.c_str()<<endl;

    }

    /***********************************流程算法**********************************/

    void DealImage(Mat &src, Mat &resImg,const int &CN2_method,const int &CN2_dilate, const int &CN2_channel, const int& CN2_threshold, const int &CN2_HSV_Color, const int &CN2_region_down, const int &CN2_region_up)
    {
        if(src.data==nullptr||src.channels()!=3)
            return;
        //Mat resImg;
        if (CN2_method == 0)
        {
            Mat vecSrc[3];
            split(src, vecSrc);
            Mat dst;
            blur(vecSrc[CN2_channel], dst, Size(50, 50));
            resImg = dyn_threshold(vecSrc[CN2_channel], dst, CN2_threshold);
        }
        else if (CN2_method == 1)
        {
            //HSV_region HSV1(RegionValue(10, 20), RegionValue(10, 20), RegionValue(10, 20));
            HSV_region HSV[2] = { HSV_region(RegionValue(0,17),RegionValue(50,255),RegionValue(20,255)),\
                HSV_region(RegionValue(108,120),RegionValue(160,200),RegionValue(140,180)) };//定义颜色：0-》金属黄 1->蓝色（20210114 add）
            _HsvSpliteMat(src, resImg, HSV[CN2_HSV_Color].Hregion, HSV[CN2_HSV_Color].Sregion, HSV[CN2_HSV_Color].Vregion);
            cvtColor(resImg, resImg, COLOR_BGR2GRAY);

        }
        else if (CN2_method == 2)
        {
            Mat vecSrc[3];
            split(src, vecSrc);
            resImg = Mat::zeros(src.size(), CV_8UC1);
            for (int i = 0; i < vecSrc[CN2_channel].rows; i++)
            {
                for (int j = 0; j < vecSrc[CN2_channel].cols; j++)
                {
                    int value = vecSrc[CN2_channel].at<uchar>(i, j);
                    if (value <= CN2_region_up && value >= CN2_region_down)
                    {
                        resImg.at<uchar>(i, j) = 255;
                    }
                }
            }
        }

        if (CN2_dilate < 0)//腐蚀
        {
            Mat structKel = getStructuringElement(MORPH_RECT, Size(abs(CN2_dilate), abs(CN2_dilate)));//矩形结构元素
            erode(resImg, resImg, structKel);
        }
        else if (CN2_dilate > 0)//膨胀
        {
            Mat structKel = getStructuringElement(MORPH_RECT, Size(abs(CN2_dilate), abs(CN2_dilate)));//矩形结构元素
            dilate(resImg, resImg, structKel);
        }

    }

    //method 0->局部二值化，立着的金属器件推荐
    //		 1->HSV方式，水平金属器件推荐
    //		 2->二值化，适用不带金属的器件
    int GetImgContours(Mat &src, std::vector<Point> &out_contour,Mat &resImg,Mat &contoursMat, int &CN2_method, const int &CN2_Type, const int &CN2_dilate, const int &CN2_channel, const int& CN2_threshold, const int &CN2_HSV_Color, const int &CN2_region_down, const int &CN2_region_up)
    {
        if (CN2_method > 2)
            CN2_method = 0;
        DealImage(src,resImg,CN2_method,CN2_dilate,CN2_channel,CN2_threshold,CN2_HSV_Color,CN2_region_down,CN2_region_up);
        if(resImg.data == nullptr)
            return  Res_Fail;

        //查找轮廓
        std::vector<std::vector<Point>> contours;
        std::vector<Vec4i> hierarchy;
        findContours(resImg, contours, hierarchy, RETR_CCOMP, CHAIN_APPROX_SIMPLE);

        int indexMax = -1;
        long lastarea = -1;
        //竖直器件 带有空心的器件 ，需要用内层轮廓来比较结果
        for (size_t i = 0; i < contours.size(); i++)
        {
            //先比较面积
            long area = contourArea(contours[i]);
            if (area > lastarea)
            {
                if (CN2_Type)//金属外部包围 轮廓为内层轮廓
                {
                    if (hierarchy[i][3] != -1)//比较是否为内层轮廓
                    {
                        lastarea = area;
                        indexMax = i;
                    }
                }
                else//一整块的连通区域
                {
                    lastarea = area;
                    indexMax = i;
                }
            }
        }

        //调试信息，查找原因
        /*Mat*/ contoursMat = Mat::zeros(resImg.size(), CV_8UC3);
        cvtColor(resImg, resImg, COLOR_GRAY2BGR);
        if (indexMax != -1)
        {
            out_contour = contours[indexMax];
            drawContours(resImg, contours, indexMax, Scalar(0, 255, 0), 5);
            drawContours(contoursMat, contours, indexMax, Scalar(0,255,0), 5);
            SaveDebugImg("contours.bmp", resImg);
        }
        else
        {
            drawContours(resImg, contours, -1, Scalar(255, 255, 255), 2);
            drawContours(contoursMat, contours, -1, Scalar(255, 255, 255), 2);
            SaveDebugImg("contours.bmp", resImg);
            return Res_Fail;
        }

        return Res_OK;
    }

    int Visual_CN2_Damage_Test(DLLFunc *pFunc, Mat detecImg, Mat goldenImg, int SpecArea, int &out_AreaStamp)//src 测试图 src1 golden图
    {
        int ret = Res_OK;
        Mat dst = detecImg;
        Mat dst1 = goldenImg.clone();

        if (dst.channels() == 3)
            cvtColor(dst, dst, COLOR_BGR2GRAY);
        if (dst1.channels() == 3)
            cvtColor(dst1, dst1, COLOR_BGR2GRAY);

        Mat result(dst.size(), dst.type());
        //Mat result = dst1 - dst;//直接相减小于0会认为是0
        //subtract(dst1, dst, result);
        for (int i = 0; i < dst.rows; i++)
        {
            for (int j = 0; j < dst.cols; j++)
            {
                result.at<uchar>(i, j) = dst1.at<uchar>(i, j) - dst.at<uchar>(i, j);
            }
        }
        //减完后交点值被减掉了，需要补回来
        Mat kenel = getStructuringElement(MORPH_RECT, Size(2, 2));//矩形结构元素
        dilate(result, result, kenel);
        SaveDebugImg("CN2_Sub.bmp", result);

        //查找轮廓
        vector<vector<Point>> contours;
        vector<Vec4i> hierarchy;
        findContours(result, contours, hierarchy, RETR_CCOMP, CHAIN_APPROX_SIMPLE);
        //findContours(result, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

        Mat contoursMat;
        cvtColor(result, contoursMat, COLOR_GRAY2BGR);
        vector<Mat> veccTemp;
        for (size_t i = 0; i < contours.size(); i++)
        {
            int area = contourArea(contours[i]);
            if (hierarchy[i][3] == -1 || contours[i].size() < 30 || area < 3000) //滤掉一些不必要的轮廓
            {
                continue;
            }
            int b = theRNG().uniform(0, 255);
            int g = theRNG().uniform(0, 255);
            int r = theRNG().uniform(0, 255);
            drawContours(contoursMat, contours, i, Scalar(b, g, r), -1, LINE_8, hierarchy, INT_MAX);
            RotatedRect rect1 = minAreaRect(contours[i]);
            Point2f center3 = rect1.center;
            putText(contoursMat, to_string(area), center3, 1, 2, Scalar(0, 255, 0));
            ShowDebugMsg(pFunc, "i:" + to_string(i) + "  contours:" + to_string(contours[i].size()) + "	Area:" + to_string(area));
            ShowDebugMsg(pFunc, "损失面积：" + to_string(area));
            if (area > SpecArea)
            {
                ShowDebugMsg(pFunc, "损失面积过大,卡关Spec为：" + to_string(SpecArea));
                ret = Res_Fail;
            }

            //记录最大面积戳
            if (area > out_AreaStamp)
            {
                out_AreaStamp = area;
            }
        }


        SaveDebugImg("CN2_Sub_Res.bmp", contoursMat);
        return ret;
    }


    /**********************************************************************************/
    void ShowError(vector<string> *outString)
    {
        QString nzs = QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%17,%18,%19,%20,%21,%22,%23,\
                      %24,%25,%26,%27,%28,%29,%30,%31,%32,%33,%34")
                .arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999)
                .arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999)
                .arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999)
                .arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999);
        outString->emplace_back(nzs.toStdString());

    }

}

/*
StringVec:
    at（0）：像素转换关系
        1：配置的需要检测项
        2：OCR或者算法需要的一些字符用；号分开
        3：是需要mm转像素的值用；分开
        ....
        4：料号
        5：位号
        6：封装
        7：spanfov
        RectVec存放所有的Rect以mm的方式存
        之前还有一些location的信息和之前一样放在后面
        RectInt是int型的，所以这部流程会在进算法前从mm转化为pix，算法直接用就好了
*/
/** @name Visual_D_CN2_Test
  @brief PU4 SMT AOI  connecter; D_CN2
/// \brief Connecter2::D_CN2
/// \param inMat           at(0):FOV图（零件pass画绿框，fail画红框的那张图）    at(1):检测图（需要检测的零件的原图）    at(2):golden图（需要用到模板匹配时的golden图）
/// \param inString        at(0):固定长度为9的字符串，每个字符内容固定为0或1。0为不检测，1为检测，一共9个检测项
//inNum参数重新定义    at(0)://method，3种检测方式，默认值1  0->局部二值化，立着的金属器件推荐 1->HSV方式，水平金属器件推荐  2->二值化，适用不带金属的器件
                       at(1)://器件类型,默认值0。 0->一块连通器件  1->器件金属在在外部包围  （可以理解为水平和竖直，因为水平器件是一块整体，而竖直器件只有外围有，内部中空）
                       at(2)://器件宽，红色框框   用于检测有无,如果找到的器件小于该轮廓的4/5，则认为是缺失----转像素
                       at(3)://器件高，红色框框   用于检测有无,如果找到的器件小于该轮廓的4/5，则认为是缺失----转像素
                       at(4)://膨胀系数，默认值0  只有外围金属轮廓不连通的情况才会使用，如需使用，推荐参数 30
                       at(5)://损件参数，器件缺失面积多少为良品,默认值15000
                       at(6)://局部二值化参数  1-> B通道图检测  2-> G通道图检测  3-> R通道图检测  默认值 3，显示3张通道的图，让操作者选择器件最亮的图填入该值
                       at(7)://局部二值化参数  二值化阈值，默认值25，一般不用修改(推荐取值范围 10 ~30)
                       at(8)://HSV参数 默认值 0， 颜色：0->土黄色   1->蓝色
                       at(9)://二值化参数，通道值     1-> B通道图检测  2-> G通道图检测  3-> R通道图检测  默认值 3，如果at(5)=0,则会显示3张通道的图，让操作者选择器件最亮的图填入该值
                       at(10)://二值化化参数区间低值  当选择了通道值后，使用ssct预处理，RBG效果，拉动对应通道，使轮廓清晰
                       at(11)://二值化化参数区间高值  同上
/// \param inRect          at(0):零件丝印层图在Fov图上的rect    at(1):零件相对于丝印层的rect  at(2):器件偏移框
/// \param inVariable     未使用
/// \param pFunc
/// \param outMat         at(0):检测结果图（将传入的 检测图*3/5 后返回）
/// \param outString      如果没有检测项fail，为空。如果有检测项fail，将对应的fail信息push
/// \param outInt         at(0):偏移Spec  at(1):偏移实测     at(2):旋转角度Spec   at(3):实测旋转角度*1000   at(4):器件中心坐标x   at(5):实测x   at(6):器件中心坐标y  at(7):实测y    at(8):器件半径   at(9):实测半径
/// \param outRect        未使用
/// \return
*/
int Visual_D_CN2_Test(vector<cv::Mat> inMat, vector<string> inString, vector<int> inNum, vector<cv::Rect> inRect,
    pstInputVar inVariable, DLLFunc *pFunc, vector<cv::Mat> *outMat, vector<string> *outString,
    vector<int> *outInt, vector<cv::Rect> *outRect)
{
    sdt::StrDebugMsg = "";
    sdt::ShowDebugMsg(pFunc, "SMT CN2接口类型器件算法检测开始");

    long startTime = 0, endTime = 0;   //算法时间
    startTime = clock();
    //参数判断
    if (inMat.size() < 3 /*|| inMat[0].data == nullptr || inMat[1].data == nullptr || inMat[2].data == nullptr \
            || inMat[0].channels() != 3 || inMat[1].channels() != 3 || inMat[2].channels() != 3*/)
    {
        sdt::ShowDebugMsg(pFunc, "inMat 数量不足3张");
        sdt::ShowError(outString);
        return VIS_Result_FAIL;
    }
    if ( inMat[0].data == nullptr || inMat[1].data == nullptr || inMat[2].data == nullptr )
    {
        sdt::ShowDebugMsg(pFunc, "inMat 图像无数据");
        sdt::ShowError(outString);
        return VIS_Result_FAIL;
    }
    if (inMat[0].channels() != 3 || inMat[1].channels() != 3 || inMat[2].channels() != 3)
    {
        sdt::ShowDebugMsg(pFunc, "inMat 图像不为3通道彩图");
        sdt::ShowError(outString);
        return VIS_Result_FAIL;
    }


    if (inString.size()<10)
    {
        sdt::ShowDebugMsg(pFunc, "inString 参数错误");
        sdt::ShowError(outString);
        return VIS_Result_FAIL;
    }
    if (inNum.size()<12)
    {
        sdt::ShowDebugMsg(pFunc, "inNum 参数错误");
        sdt::ShowError(outString);
        return VIS_Result_FAIL;
    }
    if (inRect.size()<3)
    {
        sdt::ShowDebugMsg(pFunc, "inRect 参数错误");
        sdt::ShowError(outString);
        return VIS_Result_FAIL;
    }

    /*CV_Assert(inMat.size() >= 3);
    CV_Assert(inMat[0].data != nullptr);
    CV_Assert(inMat[1].data != nullptr);
    CV_Assert(inMat[2].data != nullptr);
    CV_Assert(inMat[0].channels() == 3);
    CV_Assert(inMat[1].channels() == 3);
    CV_Assert(inMat[2].channels() == 3);
    CV_Assert(inString.size() > 7);
    CV_Assert(inNum.size() > 11);
    CV_Assert(inRect.size() > 2);*/
    /*for (int i = 0; i < inString.size(); i++)
    {
        string instring = "inString " + to_string(i) + " :" + inString[i];
        sdt::ShowDebugMsg(pFunc, std::move(instring));
    }

    for (int i = 0; i < inNum.size(); i++) {
        string instring = "inNum " + to_string(i) + " :" + to_string(inNum[i]);
        sdt::ShowDebugMsg(pFunc, std::move(instring));
    }

    for (int i = 0; i < inRect.size(); i++) {
        string instring = "inRect " + to_string(i) + " :" + to_string(inRect[i].x) + " " + to_string(inRect[i].y) + " " + to_string(inRect[i].width) + " " + to_string(inRect[i].height) + " ";
        sdt::ShowDebugMsg(pFunc, std::move(instring));
    }*/
   /* for (int i = 0; i < inRect.size(); i++) {
        string instring = "inRect " + to_string(i) + " :" + to_string(inRect[i].x) + " " + to_string(inRect[i].y) + " " + to_string(inRect[i].width) + " " + to_string(inRect[i].height) + " ";
        sdt::ShowDebugMsg(pFunc, std::move(instring));
    }
    */

    /******************************************参数部分*********************************************************/
    //检测项     0(缺件)1(偏移)2(反向)3(错料)4(损件)5(墓碑)6(异物)7(连锡短路)8(Ping脚歪斜)
    //目前检测 缺件 偏移 损件
    bool check_Missing = false;
    bool check_Offset = false;
    bool check_Damage = false;
    QString strResult = "PASS";

    //增加测试数据返回，供流程写CSV
    int vecString_Count = inString.size();
    QString liaoHao = QString::fromStdString(inString.at(vecString_Count - 4));
    QString weiHao = QString::fromStdString(inString.at(vecString_Count - 3));
    QString packType = QString::fromStdString(inString.at(vecString_Count - 2));
    QString spanFov = QString::fromStdString(inString.at(vecString_Count - 1));
    QString algName = "D_CN2";

    //新算法流程 参数定义
    int CN2_method(1);				//3种检测方式，默认值1  0->局部二值化，立着的金属器件推荐 1->HSV方式，水平金属器件推荐  2->二值化，适用不带金属的器件
    int CN2_Type(0);			   // 器件类型,默认值0。 0->一块连通器件  1->器件金属在在外部包围  （可以理解为水平和竖直，因为水平器件是一块整体，而竖直器件只有外围有，内部中空）
    int CN2_Width, CN2_Height;	  //器件宽高，红色框框   用于检测有无,如果找到的器件小于该轮廓的4/5，则认为是缺失
    int CN2_dilate(0);			 // 膨胀系数，默认值0  只有外围金属轮廓不连通的情况才会使用，如需使用，推荐参数 30
    int CN2_SpecArea(15000);	//损件参数，器件缺失面积多少为良品
    int CN2_channel(3);		   //局部二值化参数  1-> B通道图检测  2-> G通道图检测  3-> R通道图检测  默认值 3，显示3张通道的图，让操作者选择器件最亮的图填入该值
    int CN2_threshold(25);    //局部二值化参数  二值化阈值，默认值25，一般不用修改(推荐取值范围 10 ~30)
    int CN2_HSV_Color(0);    //局HSV参数 默认值 0， 颜色：0->土黄色，1->蓝色
    int CN2_region_down;	//二值化化参数区间低值  当选择了通道值后，使用ssct预处理，RBG效果，拉动对应通道，使轮廓清晰
    int CN2_region_up;	   //二值化化参数区间高值

    double specMiss(0.8); //缺件百分比

    //CSV需要写入参数定义
    QString CSVerrorMsg = "";			//错误信息
    QString CSVstandMissing = "-999";	//缺失标准
    QString CSValgMissing = "-999";		//算法实际
    QString CSVstandOffSet = "-999";	//偏移标准
    QString CSValgOffSet = "-999";		//算法实际
    QString CSVstandDamage = "-999";	//损件标准
    QString CSValgDamage = "-999";		//算法实际


    //修改传入的图的比例问题
    if (inMat.at(1).cols * 3 / 5 - inMat.at(2).cols > 5 && inMat.at(1).rows * 3 / 5 - inMat.at(2).rows > 5)
    {
        double rate = (inMat.at(1).cols*3.0 / 5.0) / inMat.at(2).cols; //放大比例
        resize(inMat.at(2).clone(), inMat.at(2), Size(0, 0), rate, rate, INTER_LINEAR);//放大golden图
    }

    //参数赋值
    CN2_method = inNum[0];
    CN2_Type = inNum[1];
    CN2_Width = inNum[2];
    CN2_Height = inNum[3];
    CN2_dilate = inNum[4];
    CN2_SpecArea = inNum[5];
    CN2_threshold = inNum[7];
    CN2_HSV_Color = inNum[8];
    CN2_region_down = inNum[10];
    CN2_region_up = inNum[11];

    if (CN2_method == 0)
        CN2_channel = inNum[6];//at(6)和at(9)共用一个参数
    else if (CN2_method == 2)
        CN2_channel = inNum[9];//at(6)和at(9)共用一个参数

    //

    CN2_SpecArea = atoi(inString[5].c_str());
    specMiss = atof(inString[4].c_str());
    sdt::ShowDebugMsg(pFunc, "缺件Spec：" + std::to_string(specMiss));
    sdt::ShowDebugMsg(pFunc, "损件Spec："+ std::to_string(CN2_SpecArea));

    //sdt::ShowDebugMsg(pFunc, "当前紫色框面积大小为：" + std::to_string(CN2_Width*CN2_Height));
   // sdt::ShowDebugMsg(pFunc, "损件面积spec可调节紫框大小来衡量参数");

    //检测项     0(缺件)1(偏移)2(反向)3(错料)4(损件)5(墓碑)6(异物)7(连锡短路)8(Ping脚歪斜)
    string strCheck = inString.at(1);
    check_Missing = ("1" == strCheck.substr(0, 1)) ? true : false;
    check_Offset = ("1" == strCheck.substr(1, 1)) ? true : false;
    //check2 = ("1" == strCheck.substr(2, 1)) ? true : false;
    check_Damage = ("1" == strCheck.substr(4, 1)) ? true : false;

    //缺件检测
    if (check_Missing)
    {
        if(inRect.size()<4)
        {
            sdt::ShowDebugMsg(pFunc, "没有粉色框，默认缺件PASS，请及时添加粉色框以保证测试结果准确性");
            strResult = "PASS";
            CSVerrorMsg = "$NO_Pink_Rect";
        }
        else {
            Mat dealImg = inMat.at(1).clone();
            Mat resImg;
            sdt::DealImage(dealImg,resImg,CN2_method,CN2_dilate,CN2_channel,CN2_threshold,CN2_HSV_Color,CN2_region_down,CN2_region_up);
            //ROI颜色占比区域
            Rect stuff_rect = Rect(inRect[3].x + dealImg.cols / 5, inRect[3].y + dealImg.rows / 5, inRect[3].width, inRect[3].height);
            CSVstandMissing = QString::number(specMiss);//缺失标准
            if(resImg.data != nullptr)
            {
                //判定Rect是否有效
                if(stuff_rect.x + stuff_rect.width >resImg.cols-1 || stuff_rect.y+stuff_rect.height> resImg.rows )
                {
                    sdt::ShowDebugMsg(pFunc, "器件框有误，请重新框选（紫色框）");
                    strResult = "FAIL";
                    CSVerrorMsg = "$Miss";
                    goto TestEnd;
                }
                else
                {
                    Mat tempImg = resImg(stuff_rect).clone();//单通道的图
                    int count = 0;
                    for (int i = 0;i<tempImg.rows-1;i++)
                    {
                        for(int j = 0;j<tempImg.cols-1;j++)
                        {
                            if(tempImg.at<uchar>(i,j))
                            {
                                count++;
                            }
                        }
                    }

                    double rate = double(count)/double((tempImg.rows-1)*(tempImg.cols-1));
                    CSValgMissing = QString::number(rate);
                    if(rate > specMiss)
                    {
                        sdt::ShowDebugMsg(pFunc, "缺件结果为PASS，缺件颜色占比：" + CSVstandMissing.toStdString() + " ，实际颜色占比：" + CSValgMissing.toStdString());
                        strResult = "PASS";
                    }
                    else
                    {
                        sdt::ShowDebugMsg(pFunc, "缺件结果为FAIL，缺件颜色占比：" + CSVstandMissing.toStdString() + " ，实际颜色占比：" + CSValgMissing.toStdString());
                        strResult = "FAIL";
                        CSVerrorMsg = "$Miss";
                        goto TestEnd;
                    }
                }
            }
            else {
                CSValgMissing = "0";
                sdt::ShowDebugMsg(pFunc, "缺件结果为FAIL，缺件颜色占比：" + CSVstandMissing.toStdString() + " ，实际颜色占比：" + CSValgMissing.toStdString());
                strResult = "FAIL";
                CSVerrorMsg = "$Miss";
                goto TestEnd;
            }
        }
    }



    /******************************************算法部分*********************************************************/
    //新算法流程
    //    Rect srcRect =Rect(inMat.at(1).cols / 5 , inMat.at(1).rows / 5 , inMat.at(1).cols *3/ 5, inMat.at(1).rows * 3/ 5);//inMat[1]内缩1/3
    //    Mat src = inMat.at(1)(srcRect);
    if( check_Offset || check_Damage )
    {
        Mat src = inMat.at(1);
        std::vector<Point> out_contour;
        Mat contoursMat, resImg;
        if (sdt::GetImgContours(src, out_contour, resImg,contoursMat, CN2_method, CN2_Type, CN2_dilate, CN2_channel, CN2_threshold, CN2_HSV_Color, CN2_region_down, CN2_region_up) == sdt::Res_OK)
        {
            //debug 将找到的轮廓画到器件上，方便查看
            std::vector<std::vector<Point>> vecPoint;
            vecPoint.push_back(out_contour);
            drawContours(inMat.at(1), vecPoint, 0, Scalar(0, 0, 255), 8);
            //有无放到外面，目的是用颜色占比方法比价，就不用找到轮廓在比较（有可能找不到轮廓，但是有器件，并且只检有无）
#if 0
            //有无和缺失  比较轮廓宽高，和golden的器件大小是否一致,不比较面积，框的区域不全是器件
            if (check_Missing)
            {
                //检测标准赋值
                CSVstandMissing = QString("%1$%2").arg(CN2_Width).arg(CN2_Height);
                Rect rectShape = boundingRect(out_contour);
                CSValgMissing = QString("%1$%2").arg(rectShape.width).arg(rectShape.height);
                if (rectShape.width < CN2_Width * specMiss || rectShape.height < CN2_Height * specMiss)//如果找到的器件小于该轮廓的 specMiss比例，则认为是缺失
                {
                    sdt::ShowDebugMsg(pFunc, "缺件结果为FAIL，标准宽高为：" + CSVstandMissing.toStdString() + " ，实际宽高为：" + CSValgMissing.toStdString());
                    strResult = "FAIL";
                    goto TestEnd;
                }
            }
            sdt::ShowDebugMsg(pFunc, "缺件结果为PASS，标准宽高为：" + CSVstandMissing.toStdString() + " ，实际宽高为：" + CSValgMissing.toStdString());
#endif

            //偏移  判断轮廓点是否在偏移框外
            if (check_Offset)
            {
                Rect OffSetRect = Rect(inRect[2].x + src.cols / 5, inRect[2].y + src.rows / 5, inRect[2].width, inRect[2].height);//偏移框，测试图会外扩1/3，所以需要平移Rect
                for (size_t i = 0; i < out_contour.size(); i++)
                {
                    Point pos = out_contour[i];
                    //inRect[2]为偏移框
                    if (pos.x >= OffSetRect.x && pos.y >= OffSetRect.y //左上角的区域
                            && pos.x <= OffSetRect.x + OffSetRect.width && pos.y <= OffSetRect.y + OffSetRect.height)//右下角的区域
                    {
                        continue;
                    }
                    else
                    {
                        //在偏移框外部，判定为FAIL
                        CSVstandOffSet = QString("%1$%2").arg(pos.x).arg(pos.y);
                        sdt::ShowDebugMsg(pFunc, "偏移结果为FAIL，找到器件点在偏移框外部，坐标：" + CSVstandOffSet.toStdString());
                        strResult = "FAIL";
                        CSVerrorMsg = "$Offset";
                        goto TestEnd;
                        //break;
                    }
                }
            }
            sdt::ShowDebugMsg(pFunc, "偏移结果为PASS");



            //损件  与golden图的轮廓做减法，判断缺失面积，并判断轮廓内的hsv平均值是否合理
            if (check_Damage)
            {
                Mat goldenImg = inMat.at(2);
                std::vector<Point> golden_out_contour;
                Mat golden_contoursMat,golden_resImg;
                if (sdt::GetImgContours(goldenImg, golden_out_contour, golden_resImg, golden_contoursMat, CN2_method, CN2_Type, CN2_dilate, CN2_channel, CN2_threshold, CN2_HSV_Color, CN2_region_down, CN2_region_up) == sdt::Res_OK)
                {
                    //重心重合之后比较面积
                    RotatedRect rrect = minAreaRect(out_contour);
                    Point2f center = rrect.center;  //测试图重心
                    rrect = minAreaRect(golden_out_contour);
                    Point2f golden_center = rrect.center;//golden图重心
                    //将测试图重心平移，和golden图重合--------------没有进行角度上的旋转，不知道有没有隐患
                    sdt::MoveImg(contoursMat, golden_center.x - center.x, golden_center.y - center.y, golden_contoursMat.size());

                    int out_AreaStamp = -999;
                    if (sdt::Visual_CN2_Damage_Test(pFunc, contoursMat, golden_contoursMat, CN2_SpecArea, out_AreaStamp) == sdt::Res_OK)
                    {
                        sdt::ShowDebugMsg(pFunc, "损件面积PASS，允许面积标准：" + to_string(CN2_SpecArea) + ",实际损失面积：" + std::to_string(out_AreaStamp));
                        CSVstandDamage = QString::number(CN2_SpecArea);
                        CSValgDamage = QString::number(out_AreaStamp);
                        //比较颜色
                        Rect rectShape = boundingRect(out_contour);
                        Rect golden_rectShape = boundingRect(golden_out_contour);
                        Mat Roi_Img = inMat.at(1)(rectShape);
                        Mat golden_Roi_Img = inMat.at(2)(golden_rectShape);

                        if (CN2_Type == 0)//一块连通区域，不仅要比较面积大小，还要比较颜色
                        {
                            int Hnum = 0, Snum = 0, Vnum = 0;
                            if (sdt::GetImg_HSV_Average_Value(Roi_Img, Hnum, Snum, Vnum) == sdt::Res_OK)
                            {
                                sdt::ShowDebugMsg(pFunc, "检测图颜色 H：" + to_string(Hnum) + " S:" + to_string(Snum) + " V:" + to_string(Vnum));
                                int golden_Hnum = 0, golden_Snum = 0, golden_Vnum = 0;
                                if (sdt::GetImg_HSV_Average_Value(golden_Roi_Img, golden_Hnum, golden_Snum, golden_Vnum) == sdt::Res_OK)
                                {
                                    sdt::ShowDebugMsg(pFunc, "golden图颜色 H：" + to_string(golden_Hnum) + " S:" + to_string(golden_Snum) + " V:" + to_string(golden_Vnum));
                                    if (abs(golden_Hnum - Hnum) < 7 && abs(golden_Snum - Snum) < 25 /*&& abs(golden_Vnum - Vnum) < 20*/)//20像素以内差异不会太高
                                    {
                                        sdt::ShowDebugMsg(pFunc, "检测PASS,面积和颜色相似");
                                        strResult = "PASS";
                                        goto TestEnd;
                                    }
                                    else
                                    {
                                        sdt::ShowDebugMsg(pFunc, "检测Fail，面积相似 、 颜色不相似");
                                        strResult = "FAIL";
                                        CSVerrorMsg = "$损件颜色不对";
                                        goto TestEnd;
                                    }

                                }
                            }
                        }

                        strResult = "PASS";
                        goto TestEnd;

                    }
                    else
                    {
                        sdt::ShowDebugMsg(pFunc, "损件结果为FAIL，允许面积标准：" + to_string(CN2_SpecArea) + ",实际损失面积：" + to_string(out_AreaStamp));
                        CSVstandDamage = QString::number(CN2_SpecArea);
                        CSValgDamage = QString::number(out_AreaStamp);
                        strResult = "FAIL";
                        CSVerrorMsg = "$损件";
                        goto TestEnd;
                    }

                }
                else
                {
                    sdt::ShowDebugMsg(pFunc, "golden图找不到轮廓，请选择正确的图重新制作golden。");
                    CSVstandDamage = QString::number(CN2_SpecArea);
                    strResult = "FAIL";
                    CSVerrorMsg = "$损件golden图不对";
                }
            }

        }
        else
        {
            //没有找到轮廓，判断为缺失
            CSVstandMissing = QString("%1$%2").arg(CN2_Width).arg(CN2_Height);
            CSValgMissing = "0$0";
            strResult = "FAIL";
            CSVerrorMsg = "Missing";
            goto TestEnd;
        }
    }

TestEnd:
    QString inOCRSpec("-999"), outOCRActural("-999");
    QString in3DSpec("-999"), out3DActural("-999");
    QString inAISpec("-999"), outAIActural("-999");
    QString inWhiteSpec("-999"),outWhiteSpec("-999");
    QString strGRR("-999");
    QString nzs = QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%17,%18,%19,%20,%21,%22,%23,%24,%25,%26,%27,%28,%29,%30,%31,%32,%33,%34")
            .arg(liaoHao).arg(weiHao).arg(packType).arg(spanFov).arg(algName).arg(strResult)
            .arg(CSVstandMissing).arg(CSValgMissing).arg(CSVstandOffSet).arg(CSValgOffSet).arg(-999).arg(-999).arg(-999).arg(-999)
            .arg(CSVstandDamage).arg(CSValgDamage).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999)
            .arg(inOCRSpec).arg(outOCRActural).arg(in3DSpec).arg(out3DActural).arg(inAISpec).arg(outAIActural).arg(inWhiteSpec).arg(outWhiteSpec).arg(strGRR).arg(CSVerrorMsg);
    //outString->at(0) = nzs.toStdString();
    outString->emplace_back(nzs.toStdString());
    outString->emplace_back(sdt::StrDebugMsg);

    //新标准需要在outstring的末尾push一个元件相对于蓝色框的偏移，没有填 “0.0”
    outString->emplace_back("0,0");

    Rect rectROi = Rect(inMat.at(1).cols / 5 , inMat.at(1).rows / 5 , inMat.at(1).cols *3/ 5, inMat.at(1).rows * 3/ 5);
    Mat outImg = inMat[1](rectROi).clone();
    outMat->emplace_back(outImg);

    return VIS_Result_OK;
}



//在图上写文字
void PutTextToImage(Mat &src,string cstr){
    if(src.channels() !=3){
        cvtColor(src.clone(),src,COLOR_GRAY2BGR);
        }
    int font_face = cv::FONT_HERSHEY_COMPLEX;
    double font_scale = 1;
    int thickness = 1;
    int baseline;
    //获取文本框的长宽
    cv::Size text_size = cv::getTextSize(cstr, font_face, font_scale, thickness, &baseline);
    //将文本框居中绘制
    cv::Point origin;
    origin.x = src.cols / 2 - text_size.width / 2;
    origin.y = src.rows / 2 + text_size.height / 2;
    cv::putText(src, cstr, origin, font_face, font_scale, cv::Scalar(0, 255, 0), thickness, 8, 0);
}


//UIC第一次三次定位算法:
//MarkMat:外扩1/3后的器件实物图，
//&OutOffset:回传的偏移坐标，
//rect:在FOV上框器件图的Rect
bool UIC_FindMark(Mat MarkMat,Point &OutOffset,Rect rect,DLLFunc *pFunc)
{
    Mat img = MarkMat.clone();
    OutOffset = Point(0,0);
    //imshow("src", img);
    Mat blurImg, grayImg;
    GaussianBlur(img, blurImg, Size(9, 9), 0.0);
    if(blurImg.channels()!=1){
        cvtColor(blurImg, grayImg, COLOR_BGR2GRAY);
    }
    Canny(grayImg.clone(), grayImg, 30, 100);
    Mat element = getStructuringElement(MORPH_RECT, Size(3, 3));
    dilate(grayImg.clone(), grayImg, element);
    //imshow("Canny_Dilate", grayImg);

    vector<vector<Point>> contours;  //轮廓定义
    vector<Vec4i> hierarcy;
    findContours(grayImg, contours, hierarcy, RETR_LIST, CHAIN_APPROX_SIMPLE);

    vector<Rect> boundRect(contours.size()); //定义外接矩形集合
    vector<Point> approx;

    for (int i = 0; i < contours.size(); i++)
    {
        //Mat temp = img.clone();
        double area = contourArea(contours[i]);
        double length = arcLength(contours[i], true);
        approxPolyDP(contours[i], approx, 0.02 * length, true);

        boundRect[i] = boundingRect(Mat(contours[i]));
        //找到实际的IC外围白色框,与golden里的框做比较(要求宽高差值在20像素内)
        if(abs(boundRect[i].width-rect.width) < 20 && abs(boundRect[i].height-rect.height) < 20){

            //判断2个矩形框的距离（要求距离差值在10像素内）
            Point newCenter=Point(boundRect[i].x+boundRect[i].width/2,boundRect[i].y+boundRect[i].height/2);
            Point oldCenter=Point(img.cols/2,img.rows/2);
            //int dis=abs(pow((pow(newCenter.x-oldCenter.x,2)+pow(newCenter.y-oldCenter.y,2)),0.5));
            //if(dis>10){
            OutOffset.x=newCenter.x-oldCenter.x;
            OutOffset.y=newCenter.y-oldCenter.y;
            //OutOffset = Point(newCenter.x-oldCenter.x,newCenter.y-oldCenter.y);
            // }
            pFunc->mDLLLogCatfunc("UIC三次定位成功",eLog_Debug);
            return true;
        }
    }
    pFunc->mDLLLogCatfunc("UIC三次定位失败",eLog_Debug);
    return false;
}

//UIC第二次三次定位算法:
//MarkMat:外扩1/3后的器件实物图
//MarkMatGolden：做golden时未外扩的器件原图
//GoldenRect:用于在实物上框取器件部分作为匹配Golden的Rect
//相似度最高匹的配点与GoldenRect起点的差值就是偏移坐标
bool UIC_FindMark2(Mat MarkMat,Mat MarkMatGolden,Point &OutOffset,DLLFunc * pFunc){

    Mat golden = MarkMatGolden;//(GoldenRect);
    double matchSpace=0.7;
    string qstr="";
    OutOffset=Point(0,0);
    //模板匹配
    Mat result;double minval, maxval=0.1;Point minpoint, maxpoint;

    //  优化模板匹配时间---20200901
    int rate=1;//偏移距离放大系数，不进行放大时就令其为1
    if(MarkMat.cols*MarkMat.rows*3 >= 10*1024*1024){

        // 大于10M的图进行缩小
        rate=2;
        // resize(MarkMat.clone(),MarkMat,Size(),0.5,0.5,INTER_AREA);//对实物图进行缩小
        // resize(golden.clone(),golden,Size(),0.5,0.5,INTER_AREA);
    }

    //imwrite("UIC_FindMark2_golden.bmp",golden);
    // imwrite("UIC_FindMark2_src.bmp",MarkMat);

    matchTemplate(MarkMat, golden, result, TM_CCOEFF_NORMED);
    minMaxLoc(result, &minval, &maxval, &minpoint, &maxpoint);
    qstr="三次定位最大相似度="+to_string(maxval)+",spec="+to_string(matchSpace);
    if (maxval>matchSpace)
    {
        OutOffset.x=(maxpoint.x-MarkMat.cols/5);
        OutOffset.y=(maxpoint.y-MarkMat.rows/5);
        pFunc->mDLLLogCatfunc("三次定位成功,"+qstr,eLog_Debug);
        return true;
    }
    else {
        pFunc->mDLLLogCatfunc(qstr,eLog_Debug);
    }
    pFunc->mDLLLogCatfunc("三次定位失败",eLog_Debug);
    return false;
}

bool UIC_FindMark2(Mat MarkMat,Mat MarkMatGolden,Rect GoldenRect,Point &OutOffset,DLLFunc * pFunc){


    Mat golden = MarkMatGolden(GoldenRect);
    double matchSpace=0.7;
    string qstr="";
    OutOffset=Point(0,0);
    //模板匹配
    Mat result;double minval, maxval=0.1;Point minpoint, maxpoint;

    //  优化模板匹配时间---20200901
    int rate=1;//偏移距离放大系数，不进行放大时就令其为1
    if(MarkMat.cols*MarkMat.rows*3 >= 10*1024*1024  ){

        // 大于10M的图进行缩小
        rate=2;
        //resize(MarkMat.clone(),MarkMat,Size(),0.5,0.5,INTER_AREA);//对实物图进行缩小
        // resize(golden.clone(),golden,Size(),0.5,0.5,INTER_AREA);
    }

    matchTemplate(MarkMat, golden, result, TM_CCOEFF_NORMED);
    minMaxLoc(result, &minval, &maxval, &minpoint, &maxpoint);
    qstr="三次定位最大相似度="+to_string(maxval)+",spec="+to_string(matchSpace);
    if (maxval>matchSpace)
    {
        OutOffset.x=(maxpoint.x-MarkMat.cols/5-GoldenRect.x);
        OutOffset.y=(maxpoint.y-MarkMat.rows/5-GoldenRect.y);
        pFunc->mDLLLogCatfunc("三次定位成功,"+qstr,eLog_Debug);
        return true;
    }
    else {
        pFunc->mDLLLogCatfunc(qstr,eLog_Debug);
    }
    pFunc->mDLLLogCatfunc("三次定位失败",eLog_Debug);
    return false;
}

//两点距离
float SegmentLength(const Point &pt1, const Point &pt2)
{
    float x = fabs(pt1.x - pt2.x);
    float y = fabs(pt1.y - pt2.y);
    float fLength = sqrt((x * x + y * y));

    return fLength;
}


//找最大的轮廓
struct RectIndex
{
    int rectSize;
    int position;
    double distance;
    Rect rect;
    Point centerPos; //中心坐标
};
//vector排序
bool Compare(RectIndex a, RectIndex b)
{
    return a.rectSize >= b.rectSize;
}


//PU4 SMT AOI 吕质电解电容(CAE) CAE_Inspect-lucus
/** @name Visual_CAE_Inspect_Test
///铝质电容(如果发现更多电容灰度不均匀，可以将图分为四份分别二值化后在拼接)
/// \brief CapCheck
/// \param inMat          at(0):FOV图（零件pass画绿框，fail画红框的那张图）    at(1):检测图（需要检测的 零件的原图）   at(2):golden图（需要用到模板匹配时的golden图）
inString.at(0);//像素转换关系---转换系数,除法
inString.at(1);//检测内容 如：00000000000000000   ..... ocr/ai/3D
inString.at(2);//OCR：选用语言（默认eng）,OCR：目标内容
inString.at(3);//是需要mm转像素的值用"；"分开：inNum.at(0/2/3/4/7/9）
...
inString.at(4);//料号
inString.at(5);//位号
inString.at(6);//封装方式
inString.at(7);//FOV ID
/// \param inNum
/// at(0):X偏移Spec----------------转毫米
/// at(1):旋转角度Spec
/// at(2):电容中心坐标x ----------------转毫米
/// at(3):电容中心坐标y ----------------转毫米
/// at(4):电容半径 ----------------转毫米
/// at(5):电容实际角度(黑色区域在 上:0, 下: 90, 左: 180.,右:270)
/// at(6):找电容轮廓的二值化阈值
/// at(7):Y偏移spec ----------------转毫米
/// at(8):一般电容：1,星形电容:2,
/// at(9):星形电容标志最小面积（pix）----------------转毫米
/// 新规范
///  --------->int
/// at(2):电容中心坐标x ----------------转毫米
/// at(3):电容中心坐标y ----------------转毫米
/// at(5):电容实际角度(黑色区域在 上:0, 下: 90, 左: 180.,右:270)
/// at(6):找电容轮廓的二值化阈值
/// at(8):一般电容：1,星形电容:2,、
//---------->stringspec
/// at(4):电容半径 ----------------转毫米
/// at(0):X偏移Spec----------------转毫米
/// at(7):Y偏移spec ----------------转毫米
/// at(1):旋转角度Spec
/// at(9):星形电容标志最小面积（pix）----------------转毫米
///
///
/// \param inRect          at(0):零件丝印层图在Fov图上的rect    at(1):零件相对于丝印层的rect, at(2):传入待测图的偏移框（不使用），at(3):golden图中器件的位置(紫框)
/// \param inVariable    未使用
/// \param pFunc          未使用
/// \param outMat         at(0):检测结果图（将传入的 检测图*3/5 后返回）
/// \param outString      at(0):所有的测试标准值和测试值（//”料号，位号，包装方式，SpanFov，算法名字，测试结果，0.5$80$60，0.6$75$60，-999，-999，10，8，-999，-999，0.5$80$60，0.6$75$60，-999，-999，0.5$80$60，0.6$75$60，-999，-999，-999，-999，异常信息”）如果没有异常信息就23个逗号，有就24个逗号
///                                 at(1):用于流程判断测试结果。如果没有检测项fail，为空。如果有检测项fail，将对应的fail信息push
/// \param outInt           at(0):偏移Spec  at(1):偏移实测     at(2):旋转角度Spec   at(3):实测旋转角度*1000   at(4):电容中心坐标x   at(5):实测x   at(6):电容中心坐标y  at(7):实测y    at(8):电容半径   at(9):实测半径
/// \param outRect        未使用
/// \return
**/
//毫米参数版
int Visual_CAE_Inspect_Test(vector<cv::Mat>inMat, vector<string> inString, vector<int> inNum, vector<cv::Rect> inRect,
                              pstInputVar inVariable, DLLFunc *pFunc, vector<cv::Mat> *outMat, vector<string> *outString,
                              vector<int> *outInt, vector<cv::Rect> *outRect)
{
    QString qstring;
    QDebugEx qdebug(pFunc);

    if (NULL == inVariable)
    {
        qstring = ("Visual_CAE_Inspect_Test inVariable is null..");
        bDebug = 0;
        writePstFuncLog(qstring, eLog_Normal,pFunc);
    }
    else
    {
        bDebug = inVariable->bValue;
        writePstFuncLog(("Visual_CAE_Inspect_Test bDebug = inVariable->bValue.."),eLog_Normal, pFunc);
    }

    unsigned long startTime = 0, endTime = 0;   //算法时间
    startTime = GetTickCount();

    Mat matCheck, matOut, matBinary, matGray, matFov, matGolden;
    Mat matCheckClone, matBinaryClone, matGrayClone;
    Rect rectFov;
    bool check0(false), check1(false), check2(false);
    bool checkResult = true;
    bool check0Result(false);//缺件检测结果
    int trueAngle(270);
    Point pos;
    int offSetX(0), offSetY(0), rotateAngle(0), inRadius(0), inThreshold;//传入的Spec
    int outOffSet(-999), outRotateAngle(-999), outX(-999), outY(-999), outRadius(-999);//算法实测出的值
    Rect rectOffSet(0, 0, 0, 0);//从外扩图上截取检测区域的Rect

    //增加测试数据返回，供流程写CSV
    QString standMissing("-999"), algMissing("-999"), standOffSet("-999"), algOffSet("-999"), standAngle("-999"), algAngle("-999");
    QString liaoHao;// = QString::fromStdString(inString.at(1));
    QString weiHao;// = QString::fromStdString(inString.at(2));
    QString packType;// = QString::fromStdString(inString.at(3));
    QString spanFov;// = QString::fromStdString(inString.at(4));
    QString algName = "CAE_Inspect";
    QString inOCRSpec("-999"), outOCRActural("-999");
    QString in3DSpec("-999"), out3DActural("-999");
    QString inAISpec("-999"), outAIActural("-999");
    QString nzs = QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%17,%18,%19,%20,%21,%22,%23,%24,%25,%26,%27,%28,%29,%30,%31,%32,%33,%34")
            .arg(liaoHao).arg(weiHao).arg(packType).arg(spanFov).arg(algName).arg("FAIL")
            .arg(standMissing).arg(algMissing).arg(-999).arg(-999).arg(standOffSet).arg(algOffSet).arg(standAngle).arg(algAngle)
            .arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999)
            .arg(inOCRSpec).arg(outOCRActural).arg(in3DSpec).arg(out3DActural).arg(inAISpec).arg(outAIActural).arg(-999).arg("NULL");

    outString->push_back(nzs.toStdString());//”料号，位号，测试结果，0.5$80$60，0.6$75$60，-999，-999，10，8，-999，-999，0.5$80$60，0.6$75$60，-999，-999，0.5$80$60，0.6$75$60，-999，-999，-999，-999”

    //判断inString的size是否会导致越界
    if(inString.size()>=13) {
        //给料号、位号等赋值
        liaoHao = QString::fromStdString(inString.at(inString.size()-4));
        weiHao = QString::fromStdString(inString.at(inString.size()-3));
        packType = QString::fromStdString(inString.at(inString.size()-2));
        spanFov = QString::fromStdString(inString.at(inString.size()-1));
        nzs = QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%17,%18,%19,%20,%21,%22,%23,%24,%25,%26,%27,%28,%29,%30,%31,%32,%33,%34")
                .arg(liaoHao).arg(weiHao).arg(packType).arg(spanFov).arg(algName).arg("FAIL")
                .arg(standMissing).arg(algMissing).arg(-999).arg(-999).arg(standOffSet).arg(algOffSet).arg(standAngle).arg(algAngle)
                .arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999)
                .arg(inOCRSpec).arg(outOCRActural).arg(in3DSpec).arg(out3DActural).arg(inAISpec).arg(outAIActural).arg(-999).arg(-999).arg(-999).arg("NULL");
        outString->at(0)=nzs.toStdString();
    }
    else {
        writePstFuncLog("error:inString.size()<13 , 越界fail",eLog_Debug, pFunc);
        nzs = QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%17,%18,%19,%20,%21,%22,%23,%24,%25,%26,%27,%28,%29,%30,%31,%32,%33,%34")
                .arg(liaoHao).arg(weiHao).arg(packType).arg(spanFov).arg(algName).arg("FAIL")
                .arg(standMissing).arg(algMissing).arg(-999).arg(-999).arg(standOffSet).arg(algOffSet).arg(standAngle).arg(algAngle)
                .arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999)
                .arg(inOCRSpec).arg(outOCRActural).arg(in3DSpec).arg(out3DActural).arg(inAISpec).arg(outAIActural).arg(-999).arg(-999).arg(-999).arg("/inString.size()<5");
        outString->at(0)=nzs.toStdString();
        return VIS_Result_FAIL;
    }

    if(inNum.size()<7){
        qstring=QString("error:inNum.size()=%1").arg(inNum.size());
        writePstFuncLog(qstring,eLog_Debug, pFunc);
        nzs = QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%17,%18,%19,%20,%21,%22,%23,%24,%25,%26,%27,%28,%29,%30,%31,%32,%33,%34")
                .arg(liaoHao).arg(weiHao).arg(packType).arg(spanFov).arg(algName).arg("FAIL")
                .arg(standMissing).arg(algMissing).arg(-999).arg(-999).arg(standOffSet).arg(algOffSet).arg(standAngle).arg(algAngle)
                .arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999)
                .arg(inOCRSpec).arg(outOCRActural).arg(in3DSpec).arg(out3DActural).arg(inAISpec).arg(outAIActural).arg(-999).arg(-999).arg(-999).arg("/inNum.size()<10");
        outString->at(0)=nzs.toStdString();
        return VIS_Result_FAIL;
    }
    else{
        double TransRate=QString::fromStdString(inString.at(0)).toDouble();

        QString totalstr=QString::fromStdString(inString.at(3));
        QStringList qslist=totalstr.split(";");
        if(qslist.size()<6){
            qstring="MM转pix参数缺失";
            pFunc->mDLLLogCatfunc(qstring.toStdString(),eLog_Debug);
            VIS_Result_FAIL;
        }
        else {
//            inNum.at(0)=int(qslist.at(0).toDouble()/TransRate);
//            inNum.at(2)=int(qslist.at(1).toDouble()/TransRate);
//            inNum.at(3)=int(qslist.at(2).toDouble()/TransRate);
//            inNum.at(4)=int(qslist.at(3).toDouble()/TransRate);
//            inNum.at(7)=int(qslist.at(4).toDouble()/TransRate);
//            inNum.at(9)=int(qslist.at(5).toDouble()/(TransRate));

            //新规范
            inNum.at(0) = int(qslist.at(0).toDouble()/TransRate);
            inNum.at(1) = int(qslist.at(1).toDouble()/TransRate);
            inString.at(4) = to_string(int(qslist.at(2).toDouble()/TransRate));
            inString.at(5) = to_string(int(qslist.at(3).toDouble()/TransRate));
            inString.at(6) = to_string(int(qslist.at(4).toDouble()/TransRate));
            inString.at(8) = to_string(int(qslist.at(5).toDouble()/TransRate));
        }
    }


    //通过参数数量判断是一般电容还是星形电容,两种参数数量都是对的
    if(inMat.size() > 2 && inNum.size()>=7 && inRect.size()>2)
    {

        //判断golden图是否进行了缩放
        if(inMat.at(1).cols*3/5 - inMat.at(2).cols > 5 &&  inMat.at(1).rows*3/5 - inMat.at(2).rows > 5){
            double rate=(inMat.at(1).cols*3.0/5.0) / inMat.at(2).cols; //放大比例
            resize(inMat.at(2).clone(),inMat.at(2),Size(0,0),rate,rate,INTER_LINEAR );//放大golden图
            writePstFuncLog("do resize ok",eLog_Debug, pFunc);
        }
        else {
            writePstFuncLog("not resize",eLog_Debug, pFunc);
        }

        matFov = inMat.at(0);
        matGolden = inMat.at(2).clone();

        //strDirection = inString.at(0);
        //检测项     0(缺件)1(偏移)2(反向)3(错料)4(损件)5(墓碑)6(异物)7(连锡短路)8(Ping脚歪斜)
        string strCheck = inString.at(1);

        check0 = ("1" == strCheck.substr(0, 1)) ? true : false;
        check1 = ("1" == strCheck.substr(1, 1)) ? true : false;
        check2 = ("1" == strCheck.substr(2, 1)) ? true : false;

        rectFov = inRect.at(0);

        /// \param inNum
        /// 新规范
        ///  --------->int
        /// at(2):电容中心坐标x ----------------转毫米
        /// at(3):电容中心坐标y ----------------转毫米
        /// at(5):电容实际角度(黑色区域在 上:0, 下: 90, 左: 180.,右:270)
        /// at(6):找电容轮廓的二值化阈值
        /// at(8):一般电容：1,星形电容:2,、
        //---------->stringspec
        /// at(4):电容半径 ----------------转毫米 --------4
        /// at(0):X偏移Spec----------------转毫米 --------5
        /// at(7):Y偏移spec ----------------转毫米--------6
        /// at(1):旋转角度Spec----------------------------7
        /// at(9):星形电容标志最小面积（pix）----------------转毫米 ----8
        ///
        ///
        offSetX = atoi(inString.at(5).c_str());
        offSetY = atoi(inString.at(6).c_str());
        rotateAngle = atoi(inString.at(7).c_str());
        pos.x = inNum.at(0);
        pos.y = inNum.at(1);
        inRadius = atoi(inString.at(4).c_str());
        trueAngle = inNum.at(2);
        inThreshold = inNum.at(3);

/*
        offSetX = inNum.at(0);
        rotateAngle = inNum.at(1);
        pos.x = inNum.at(2);
        pos.y = inNum.at(3);
        inRadius = inNum.at(4);
        trueAngle = inNum.at(5);
        inThreshold = inNum.at(6);
        if ( inNum.size() > 7 )
        {
            offSetY = inNum.at(7);
        }
        else
        {
            offSetY = offSetX;
        }
*/


    }
    else
    {
        qdebug<< "wrong param";
        nzs = QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%17,%18,%19,%20,%21,%22,%23,%24,%25,%26,%27,%28,%29,%30,%31,%32,%33,%34")
                .arg(liaoHao).arg(weiHao).arg(packType).arg(spanFov).arg(algName).arg("FAIL")
                .arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999)
                .arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999)
                .arg(inOCRSpec).arg(outOCRActural).arg(in3DSpec).arg(out3DActural).arg(inAISpec).arg(outAIActural).arg(-999).arg(-999).arg(-999).arg("error:Wrong Param");
        outString->at(0) = nzs.toStdString();


        QString qstr=QString("inMat.size()=%1,inString.size()=%2,inNum.size()=%3,inRect.size()=%4").arg(inMat.size()).arg(inString.size()).arg(inNum.size()).arg(inRect.size());
        writePstFuncLog(qstr, eLog_Normal,pFunc);

        outString->push_back("error: wrong param");
        outMat->push_back(matGolden);
        return VIS_Result_FAIL;
    }

    //判断golden元件框rect相对于golden图是否越界
    if((inRect.at(3).x < 0  ||inRect.at(3).y<0  ||inRect.at(3).x+inRect.at(3).width>inMat.at(2).cols || inRect.at(3).y+inRect.at(3).height>inMat.at(2).rows)){
        qstring=QString("golden图元件框rect相对golden图越界，inRect.at(3)=%1,%2,%3,%4 ; 内缩图(WH)=%5,%6").arg(inRect.at(3).x).arg(inRect.at(3).y).arg(inRect.at(3).width).arg(inRect.at(3).height).arg(inMat.at(2).cols).arg(inMat.at(2).rows);
        writePstFuncLog(qstring,eLog_Debug, pFunc);
        nzs = QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%17,%18,%19,%20,%21,%22,%23,%24,%25,%26,%27,%28,%29,%30,%31,%32,%33,%34")
                .arg(liaoHao).arg(weiHao).arg(packType).arg(spanFov).arg(algName).arg("FAIL")
                .arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999)
                .arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999)
                .arg(inOCRSpec).arg(outOCRActural).arg(in3DSpec).arg(out3DActural).arg(inAISpec).arg(outAIActural).arg(-999).arg(-999).arg(-999).arg("error:OutOfRange");
        outString->at(0) = nzs.toStdString();
        outString->push_back("/golden图元件框rect相对golden图越界");
        return VIS_Result_FAIL;
    }
    else{
        writePstFuncLog("inRect.at(3) check ok",eLog_Debug, pFunc);
    }

    if(inMat.at(1).empty() || matGolden.empty())
    {
        qdebug<< "empty Mat";
        nzs = QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%17,%18,%19,%20,%21,%22,%23,%24,%25,%26,%27,%28,%29,%30,%31,%32,%33,%34")
                .arg(liaoHao).arg(weiHao).arg(packType).arg(spanFov).arg(algName).arg("FAIL")
                .arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999)
                .arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999)
                .arg(inOCRSpec).arg(outOCRActural).arg(in3DSpec).arg(out3DActural).arg(inAISpec).arg(outAIActural).arg(-999).arg(-999).arg(-999).arg("error: empty Mat");
        outString->at(0) = nzs.toStdString();
        outString->push_back("error: empty Mat");
        outMat->push_back(matGolden);
        return VIS_Result_FAIL;
    }
    else
    {
        Point offset;
        Mat ReturnMat;

        if( false && UIC_FindMark2(inMat.at(1),inMat.at(2),inRect.at(2),offset,pFunc) )
        {
            if((inMat.at(1).cols/5+offset.x+inMat.at(1).cols*3/5) > inMat.at(1).cols || (inMat.at(1).rows/5+offset.y+inMat.at(1).rows*3/5) > inMat.at(1).rows || inMat.at(1).cols/5+offset.x < 0 ||inMat.at(1).rows/5+offset.y < 0)
            {
                qdebug<< "三次定位内缩越界";
                nzs = QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%17,%18,%19,%20,%21,%22,%23,%24,%25,%26,%27,%28,%29,%30,%31,%32,%33,%34")
                        .arg(liaoHao).arg(weiHao).arg(packType).arg(spanFov).arg(algName).arg("FAIL")
                        .arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999)
                        .arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999)
                        .arg(inOCRSpec).arg(outOCRActural).arg(in3DSpec).arg(out3DActural).arg(inAISpec).arg(outAIActural).arg(-999).arg(-999).arg(-999).arg("error:OutOfRange");
                outString->at(0) = nzs.toStdString();
                outString->push_back("三次定位内缩越界");
                outMat->push_back(matGolden);
                return VIS_Result_FAIL;
            }

            rectOffSet = Rect(inMat.at(1).cols/5+offset.x,inMat.at(1).rows/5+offset.y,inMat.at(1).cols*3/5,inMat.at(1).rows*3/5);
            ReturnMat=inMat.at(1)(Rect(inMat.at(1).cols/5+offset.x,inMat.at(1).rows/5+offset.y,inMat.at(1).cols*3/5,inMat.at(1).rows*3/5));
            rectangle(inMat.at(0),Rect(inRect.at(0).x+offset.x,inRect.at(0).y+offset.y,inRect.at(0).width,inRect.at(0).height),Yellow,8,8,0);//在fov上画新找到的检测区
        }
        else
        {
            rectOffSet = Rect(inMat.at(1).cols/5,inMat.at(1).rows/5,inMat.at(1).cols*3/5,inMat.at(1).rows*3/5);
            ReturnMat=inMat.at(1)(Rect(inMat.at(1).cols/5,inMat.at(1).rows/5,inMat.at(1).cols*3/5,inMat.at(1).rows*3/5));
        }
        matCheck = ReturnMat.clone(); //定位内缩后的图
    }

    //outInt默认返回值
    for ( unsigned int i = 0; i < 10; i++)
    {
        outInt->push_back(-999);
    }


    /*************数据初始化完成，开始测试。。。。。。。**************/
    /***********************************************************************/
    if(1 != matCheck.channels())
    {
        cvtColor(matCheck, matGray, COLOR_BGR2GRAY);
    }
    else
    {
        matGray = matCheck.clone();
    }

    standMissing = QString::number(inRadius);
    standOffSet = QString::number(offSetX) + "$" + QString::number(offSetY);
    standAngle = QString::number(rotateAngle);
    QString errorMsg = "";

    matCheckClone = matCheck.clone();

    //图像预处理
    //    Mat kernel = (Mat_<float>(3,3) << -2, -1, 0, -1, 1, 1, 0, 1, 2);  //浮雕图
    //    filter2D(matGray, matOut, -1, kernel);

    //    threshold(matOut, matBinary, 108, 255, THRESH_BINARY | THRESH_OTSU);
    threshold(matGray, matBinary, inThreshold, 255, THRESH_BINARY);
    //imshow("Binary", matBinary);
    //    Mat element = getStructuringElement(MORPH_RECT, Size(7,7));
    //    erode(matBinaryClone, matBinaryClone, element);//腐蚀

    int minSize = 3 * inRadius * inRadius;
    QString msg = "";
    Point3f circleInfo;  //电容所在圆的信息
    //初始化为原图坐标，缺件时截取原图
    circleInfo.x = pos.x;
    circleInfo.y = pos.y;

    int idx = 0;
    ///*************有无检测(缺件)***********//  找轮廓
    Mat debugImg;
    cvtColor(matGray,debugImg,COLOR_GRAY2BGR);
    for ( ; idx < 6; idx++ )
    {
        qdebug<< "idx:";
        qdebug<< "idx:" << to_string(idx);
        if ( idx < 3 )
        {
            inThreshold = inThreshold - idx * 10;
            threshold(matGray, matBinary, inThreshold, 255, THRESH_BINARY);
        }
        else
        {
            inThreshold = inThreshold + idx * 10;
            threshold(matGray, matBinary, inThreshold, 255, THRESH_BINARY);
        }

        if(check0)
        {
            qdebug<< "size Check";
            vector<vector<Point>> contours;
            vector<Vec4i> hierarchy;
            int sizeNum = 0;  //大小符合条件的矩形数量
            vector<RectIndex>rect1;
            RectIndex eachRect1;
            float rectWidth(0.0), rectHeight(0.0);

            //图片的四条边边上的白色像素个数和超过1/4就认为被遮挡
            int xs(1), xe(1), ys(1), ye(1);
            int x41 = matGray.cols / 4;
            int y41 = matGray.rows / 4;
            for ( int i = x41; i < (matGray.cols - x41); i++ )
            {
                if ( 250 < matGray.at<uchar>(0, i) )//第一行
                {
                    xs++;
                   // circle(debugImg,)
                }

                if ( 250 < matGray.at<uchar>(matGray.rows - 1, i) )//最后一行
                {
                    xe++;
                }
            }

            for ( int i = y41; i < (matBinary.rows - y41); i++ )
            {
                if ( 250 < matGray.at<uchar>(i, 0) )//第一列
                {
                    ys++;
                }

                if ( 250 < matGray.at<uchar>(i, matGray.cols - 1) )//最后一列
                {
                    ye++;
                }
            }

            qdebug<< "matGray:"<< to_string(matGray.cols)<< ","<< to_string(matGray.rows);
            qdebug<< "xs:"<< to_string(xs);
            qdebug<< "xe:"<< to_string(xe);
            qdebug<< "ys:"<< ys;
            qdebug<< "ye:"<< ye;
            if ( matGray.cols / xs < 2 * 2 || matGray.cols / xe < 2 * 2 || matGray.rows / ys < 2 * 2 || matGray.rows / ye < 2 * 2 )
            {
                qdebug<< "zhedang fail";
                msg = "||||||||";
                PutTextToImage(matCheckClone, msg.toStdString());
                sizeNum = 0;
            }
            else
            {
                qdebug<< "capInSize: "<< minSize;
                findContours( matBinary, contours, hierarchy, RETR_CCOMP, CHAIN_APPROX_SIMPLE );
                for (unsigned int i = 0; i < contours.size(); i++)
                {
                    RotatedRect rotateRect = minAreaRect(contours[i]);
                    Rect rect = boundingRect(contours[i]);
                    Point2f P[4];
                    rotateRect.points(P);
                    rectWidth = SegmentLength(P[0], P[1]);
                    rectHeight = SegmentLength(P[1], P[2]);
                    /*
                    rectangle(matBinaryClone, rect, Green);
                                if ( rect.width * rect.height > (minSize - 50) && abs(rect.width - rect.height) )
                                {
                                    eachRect1.rectSize = rect.width * rect.height;
                                    eachRect1.position = i;
                                    eachRect1.rect = rect;
                                    eachRect1.centerPos = Point(rect.x + rect.width / 2, rect.y + rect.height / 2);
                                    rect1.push_back(eachRect1);

                                    sizeNum++;
                                }
                                if((rectWidth * rectHeight > (minSize - 50)))
                                {
                                    qdebug<< "rectWidth:"<< rectWidth;
                                    qdebug<< "rectHeight:"<< rectHeight;
                                }
                    */
                    if((rectWidth * rectHeight > (minSize - 50)) && abs(rectWidth - rectHeight) < 50) //找到电容  面积，长宽比符合条件
                    {
                        int whiteNum = 0;
                        int totalNum = 0;
                        for ( int idy = rect.y; idy < rect.y + rect.height; idy++ )
                        {
                            for ( int idx = rect.x; idx < rect.x + rect.width; idx++ )
                            {
                                totalNum++;
                                if ( matBinary.at<uchar>(idy, idx) > 100 )
                                {
                                    whiteNum++;
                                }
                            }
                        }
                        //                    qdebug << "whiteNum:"<< whiteNum;
                        //                    qdebug<< "totalNum:"<< totalNum;
                        //                    qdebug<< ":"<< (float)whiteNum / totalNum;
                        if ( (float)whiteNum / totalNum < 0.5 )//找到的轮廓中白色像素个数至少要占一半以上
                        {
                            continue;
                        }
                        eachRect1.rectSize = rectWidth * rectHeight;
                        eachRect1.position = i;
                        eachRect1.rect = rect;
                        eachRect1.centerPos = rotateRect.center;
                        rect1.push_back(eachRect1);

                        sizeNum++;
                    }
                }//for(fingContours)
            }

            if(0 == sizeNum && 5 == idx ) //最后一次才算缺件
            {
                msg = "Missing Part no square";
                qdebug<< "Missing Part no square";
                PutTextToImage(matCheckClone, msg.toStdString());

                outString->push_back("MissingPart");
                outRadius = 0;
                algMissing = QString::number(0);
                errorMsg = "MissingPart";
                checkResult = false;
                check0Result = false;
            }
            else if (sizeNum >= 1)
            {
                sort(rect1.begin(), rect1.end(), Compare);
                RotatedRect rect = minAreaRect(contours[rect1.at(sizeNum -1).position]);//找到多个符合条件的轮廓，取最大的当做电容
                Point2f P[4];
                Point center;
                float radius;
                rect.points(P);
                rectWidth = SegmentLength(P[0], P[1]);
                rectHeight = SegmentLength(P[1], P[2]);
                center = rect.center;
                radius = (rectWidth + rectHeight) / 4;
                Point3f p(center.x, center.y, radius);//矩形内接圆坐标
                circleInfo = p;
                outRadius = sqrt((rectWidth * rectHeight) / 3);
                algMissing = QString::number(outRadius);
                check0Result = true;
                qdebug<< "check0 pass";
                break;
            }
        }
    }


    ///*******偏移检测(偏移)********//  根据最大轮廓的中心检测
    Point algPos(circleInfo.x, circleInfo.y);  //算法找到的圆心坐标
    int getX(0), getY(0);//x, y方向的偏移
    if( check1 && check0Result )
    {
        qdebug<< "offset Check";

        double getOffset = static_cast<double>(SegmentLength(algPos, pos));
        getX = abs(algPos.x - pos.x);
        getY = abs(algPos.y - pos.y);
        algOffSet = QString::number(getX) + "$" + QString::number(getY);
        qdebug<< "get offSet X:"<< getX << "  Y:"<<getY;

        qdebug<< circleInfo.x;
        qdebug<< circleInfo.y;
        qdebug<< circleInfo.z;
        qdebug<< "x= "<< algPos.x;
        qdebug<< "y= "<< algPos.y;

        QString ttt = QString("algPos(%1,%2), pos(%3,%4), matCheckClone(%5,%6)").arg(algPos.x).arg(algPos.y).arg(pos.x).arg(pos.y).arg(matCheckClone.cols).arg(matCheckClone.rows);
        //        DebugLogLXS("Visual_CAE_Inspect_Test: " + ttt);
        if ( algPos.x  > matCheckClone.cols || algPos.y > matCheckClone.rows || pos.x >matCheckClone.cols || pos.y > matCheckClone.rows) //防止越界
        {
            qdebug<< "algPos Error";
            msg = "minoffset:" + QString::number(getOffset);
            qdebug<< "offset fail: "<< getOffset;
            //PutTextToImage(matCheckClone, msg.toStdString());
            checkResult = false;
            outString->push_back("OffSet");
            if ( errorMsg.isEmpty() )
            {
                errorMsg = "OffSet";
            }
            else
            {
                errorMsg = errorMsg + "&OffSet";
            }
        }//if ( 越界判定fail )
        else
        {
            circle(matCheckClone, Point(circleInfo.x, circleInfo.y), circleInfo.z, Orange, 3);//找到的电容轮廓
            //            matCheckClone.at<Vec3b>(algPos)[0] = 0;
            //            matCheckClone.at<Vec3b>(algPos)[1] = 255;
            //            matCheckClone.at<Vec3b>(algPos)[2] = 0;
            circle(matCheckClone, algPos, 3, Orange, 3);//找到的电容中心点

            //            matCheckClone.at<Vec3b>(pos)[0] = 0;
            //            matCheckClone.at<Vec3b>(pos)[1] = 255;
            //            matCheckClone.at<Vec3b>(pos)[2] = 0;
            circle(matCheckClone, pos, 2, Green, 3);//实际中心点


            QString qMinoffSet = "OffSet:minoffset:" + QString::number(getOffset) + ", input offset:" + QString::number(offSetX);
            if(getX > offSetX || getY > offSetY )
            {
                msg = "minoffset:" + QString::number(getOffset);
                qdebug<< "offset fail: "<< "OffSet:minoffset:" << getOffset <<", input offset:" << offSetX;
                //PutTextToImage(matCheckClone, msg.toStdString());
                checkResult = false;
                outString->push_back("OffSet");
                if ( errorMsg.isEmpty() )
                {
                    errorMsg = "OffSet";
                }
                else
                {
                    errorMsg = errorMsg + "&OffSet";
                }
            }
            else
            {
                qdebug<< "OffSet Pass";
            }
        }

        //imshow("circle", matCheckClone);
        outX = algPos.x;
        outY = algPos.y;
        outOffSet = getOffset;
    }

    ///************旋转检测（反向）*************//截取圆的最小外接矩形

    Rect maxRect;
    Rect rect;
    Rect rect2;
    bool normal_cae=true;//true:正常电容， false：奔驰电容
    if( check2 && check0Result )
    {
        qdebug<< "Check direction";
        //DebugLogLXS("Visual_CAE_Inspect_Test direction Check...");

        //新规范
        if(inNum.at(4)==2)
            normal_cae=false;
        else
            normal_cae=true;

//        //先DB参数判断是否能检星形电容，再判断是否要检星形电容
//        if(inNum.size()> 9 && inRect.size() > 3){

//            if(inNum.at(8)==2){
//                normal_cae=false;
//            }
//            else if (inNum.at(8)==1) {
//                normal_cae=true;
//            }
//            else {
//                outString->push_back("电容类型设置错误");
//                //电容类型设置错误
//            }
//        }


        //一般电容检测方向
        if(normal_cae){
            //正常电容检测方向
            if ( circleInfo.z > 5 )
            {
                circle(matGray, Point(circleInfo.x, circleInfo.y), (circleInfo.z - 5), White, (circleInfo.z / 15), 8, 0);//避免二值化后找不到轮廓
            }

            Point pointRect(circleInfo.x - circleInfo.z, circleInfo.y - circleInfo.z);//圆的最小外接矩形的左上角在原图上的坐标
            Rect rectCircle = Rect(pointRect.x, pointRect.y, (2 * circleInfo.z - 1), (2*circleInfo.z - 1));
            if ( pointRect.x < 0 )
            {
                pointRect.x = 0;
                rectCircle.x = 0;
            }
            if ( pointRect.y < 0 )
            {
                pointRect.y = 0;
                rectCircle.y = 0;
            }
            if ( ( pointRect.x + (2 * circleInfo.z - 1)) > matGray.cols )
            {
                rectCircle.width = matGray.cols - rectCircle.x - 1;
            }
            if ( (pointRect.y + (2 * circleInfo.z - 1)) > matGray.rows )
            {
                rectCircle.height = matGray.rows - rectCircle.y - 1;
            }
            qdebug<< "matGray.rows: "<< matGray.rows;
            qdebug<< "matGray.clos: "<< matGray.cols;
            QString cqs = QString("rectCircle(%1,%2,%3,%4)").arg(rectCircle.x).arg(rectCircle.y).arg(rectCircle.width).arg(rectCircle.height);
            qdebug<< cqs.toStdString();
            Mat matRect = matGray(rectCircle);//电容所在轮廓的圆的最小外接矩形

            qdebug<< circleInfo.z<< "1.5 * circleInfo.z * circleInfo.z"<< 1.5 * circleInfo.z * circleInfo.z;
            //避免过曝的图片无法检测
            long totalGray = 0;
            long totalNum = 0;
            long whiteNum = 0;
            //当灰度值大于250的像素点个数超过圆的一半时，认为过曝，使用另一个二值化阈值
            long totalNumW = 0;
            long totalGrayW = 0;
            Mat matRectBinary;
            for (int i = 0; i < matRect.cols; i++)
            {
                for (int j = 0; j < matRect.rows; j++)
                {
                    if (SegmentLength(Point(matRect.cols / 2, matRect.rows / 2), Point(i, j)) > circleInfo.z)
                    {
                        matRect.at<uchar>(j, i) = 0; //把圆外面的像素置为黑色
                        continue;
                    }

                    if ( matRect.at<uchar>(j, i) > 235 )///判定大于235的计入白色点(判断图片是否属于过曝)
                    {
                        whiteNum++;
                    }
                    totalNumW++;
                    totalGrayW += matRect.at<uchar>(j, i);

                    if ( matRect.at<uchar>(j, i) > 250 || matRect.at<uchar>(j, i) < 5 ) //排除极大值和极小值的影响
                    {
                        continue;
                    }
                    totalGray += matRect.at<uchar>(j, i);
                    totalNum++;
                }
            }

            qdebug<< "matRect.rows:"<< matRect.rows;
            qdebug<< "matRect.cols:"<< matRect.cols;
            //imshow("ywh", matRect);
            int aveGray = totalGray / totalNum - 30;
            int aveGrayW = totalGrayW / totalNumW;
            qdebug<< "whiteNum="<< whiteNum;
            qdebug<< "totalNumW="<< totalNumW;
            qdebug<< "aveGray="<< aveGray;
            qdebug<< "aveGrayW="<< aveGrayW;
            if ( whiteNum > totalNumW / 2 )
            {
                aveGray = aveGrayW - 20;
            }
            qdebug<< "aveGray="<< aveGray;

            threshold(matRect, matRectBinary, aveGray, 255, THRESH_BINARY);

            //        adaptiveThreshold(matRect, matRectBinary, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY, 63, 5);
            int elementSize = 0; //膨胀内核大小
            if ( inThreshold < 100 )
            {
                if ( inRadius < 50 )
                {
                    elementSize = 3;
                }
                else if (inRadius < 70)
                {
                    elementSize = 7;
                }
                else if ( inRadius < 120 )
                {
                    elementSize = 11;
                }
                else if (inRadius < 180 )
                {
                    elementSize = 13;
                }
                else if ( inRadius < 240)
                {
                    elementSize = 17;
                }
                else
                {
                    elementSize = 19;
                }
            }
            else
            {
                elementSize = 3;
            }
            Mat element = getStructuringElement(MORPH_RECT, Size(elementSize, elementSize));
            dilate(matRectBinary, matRectBinary, element); //膨胀
            //imshow("matRectBinary", matRectBinary);

            vector<Point3f> result;
            vector<vector<Point> > contoursR;
            vector<Vec4i> hierarchyR;
            vector<RectIndex>rectAAA;
            RectIndex eachRect;
            Point rectPos;
            int rectSize = 0;

            findContours( matRectBinary, contoursR, hierarchyR, RETR_CCOMP, CHAIN_APPROX_SIMPLE );
            for (int i = 0; i<contoursR.size(); i++)
            {
                RotatedRect rect = minAreaRect(contoursR[i]);
                Point2f P[4];
                rect.points(P);
                float rectWidth = SegmentLength(P[0], P[1]);
                float rectHeight = SegmentLength(P[1], P[2]);

                if ( rectWidth * rectHeight < (int)(1.5 * circleInfo.z * circleInfo.z) )//判定月牙部分轮廓面积小于电容所在圆半径的一半的平方
                {
                    //                for (int j = 0; j <= 3; j++)
                    //                {
                    //                    line(matRect, P[j], P[(j + 1) % 4], Green, 3);
                    //                }
                    if (rectSize < rectWidth * rectHeight)
                    {
                        rectSize = (int)rectWidth * rectHeight;
                        //qdebug<< "max contours:"<< rectSize;

                        eachRect.rectSize = contoursR[i].size();
                        eachRect.position = i;
                        rectAAA.push_back(eachRect);
                    }
                }
            }

            float getAngle = 0.0;
            QString qDirect = "";
            if(rectAAA.size() > 0)
            {
                RotatedRect rect = minAreaRect(contoursR[eachRect.position]); //最大的轮廓
                Point2f P[4];
                rect.points(P);

                for(int i = 0; i <=3; i++)
                {
                    P[i].x = P[i].x + pointRect.x;
                    P[i].y = P[i].y + pointRect.y;
                }
                //                    matCheckClone = matCheck.clone();


                rectPos.x = rect.center.x + pointRect.x;
                rectPos.y = rect.center.y + pointRect.y;

                QString ttt = QString("rectPos(%1,%2), matCheckClone(%3,%4)").arg(rectPos.x).arg(rectPos.y).arg(matCheckClone.cols).arg(matCheckClone.rows);
                //DebugLogLXS("Visual_CAE_Inspect_Test: " + ttt);
                if ( rectPos.x > matCheckClone.cols || rectPos.y > matCheckClone.rows )
                {
                    //DebugLogLXS("Visual_CAE_Inspect_Test at youwenti");
                }
                else
                {
                    //                matCheckClone.at<Vec3b>(rectPos)[0] = 0;
                    //                matCheckClone.at<Vec3b>(rectPos)[1] = 0;
                    //                matCheckClone.at<Vec3b>(rectPos)[2] = 255;
                    circle(matCheckClone, rectPos, 2, Orange, 3);
                    //imshow("rect", matCheckClone);
                }

                msg = "angle: " + QString::number(rect.angle);
                qdebug<< "get "<< msg.toStdString();
                getAngle = min(abs(rect.angle), abs(rect.angle + 90));

                qdebug<< "rectPos(x,y) "<< rectPos.x<< " , "<< rectPos.y;
                qdebug<< "algPos(x,y)"<< algPos.x<< " , "<< algPos.y;

                bool result = true;
                if(0 == trueAngle) //月牙区域在上
                {
                    if( rectPos.y < algPos.y && abs(rectPos.y - algPos.y) > inRadius / 4 && abs(rectPos.x - algPos.x) < inRadius / 2
                            && (abs(rect.angle + 90) < rotateAngle || abs(rect.angle) < rotateAngle) )
                    {
                        qdebug<< "direction up pass";
                        //                    qDirect = "UP";
                        qDirect = "2";
                    }
                    else
                    {
                        qdebug<< "direction up fail";
                        result = false;
                    }
                }
                else if(90 == trueAngle) //下
                {
                    if( rectPos.y > algPos.y && abs(rectPos.y - algPos.y) > inRadius / 4 && abs(rectPos.x - algPos.x) < inRadius / 2
                            && (abs(rect.angle + 90) < rotateAngle || abs(rect.angle) < rotateAngle) )
                    {
                        qdebug<< "direction down pass";
                        //                    qDirect = "DOWN";
                        qDirect = "4";
                    }
                    else
                    {
                        qdebug<< "direction down fail";
                        result = false;
                    }
                }
                else if(180 == trueAngle) //左
                {
                    if( rectPos.x < algPos.x && abs(rectPos.x - algPos.x) > inRadius / 4 && abs(rectPos.y - algPos.y) < inRadius / 2
                            && (abs(rect.angle + 90) < rotateAngle || abs(rect.angle) < rotateAngle) )
                    {
                        qdebug<< "direction left pass";
                        //                    qDirect = "LEFT";
                        qDirect = "3";
                    }
                    else
                    {
                        qdebug<< "direction left fail";
                        result = false;
                    }
                }
                else if(270 == trueAngle) //右
                {
                    if( rectPos.x > algPos.x && abs(rectPos.x - algPos.x) > inRadius / 4 && abs(rectPos.y - algPos.y) < inRadius / 2
                            && (abs(rect.angle + 90) < rotateAngle || abs(rect.angle) < rotateAngle) )
                    {
                        qdebug<< "direction right pass";
                        //                    qDirect = "RIGHT";
                        qDirect = "1";
                    }
                    else
                    {
                        qdebug<< "direction right fail";
                        result = false;
                    }
                }

                if(!result)
                {
                    checkResult = false;
                    outString->push_back("WrongDirection");
                    if ( errorMsg.isEmpty() )
                    {
                        errorMsg = "WrongDirection";
                    }
                    else
                    {
                        errorMsg = errorMsg + "&WrongDirection";
                    }

                    for (int j = 0; j <= 3; j++)
                    {
                        line(matCheckClone, P[j], P[(j + 1) % 4], Red, 3);
                    }
                }
                else
                {
                    qdebug<< "WrongDirection Pass";
                    for (int j = 0; j <= 3; j++)
                    {
                        line(matCheckClone, P[j], P[(j + 1) % 4], Green, 3);
                    }
                }
            }
            else
            {
                qdebug<< "find no countours";
                PutTextToImage(matRect, "no contours");
                checkResult = false;
                outString->push_back("WrongDirection");
                if ( errorMsg.isEmpty() )
                {
                    errorMsg = "WrongDirection";
                }
                else
                {
                    errorMsg = errorMsg + "&WrongDirection";
                }
            }//没找到月牙，fail

            outRotateAngle = getAngle;
            algAngle = QString::number(getAngle, 'f', 2);
        }
        //奔驰电容检测方向---框器件时贴着框
        else {

            //0.全程使用外扩图操作
            //1.使用HSV提取电容表面，之后利用模板匹配进行定位,确定电容标志检测区(底边上下100pix,底边中心为中心宽度2/3)
            //2.使用HSV对电容标志（灰色区）进行提取
            //3.剔除丝印层干扰（轮廓宽度<检测区宽度,轮廓高度<检测区高度, 面积大于设定值）
            int minAreaSpec=0;
            //minAreaSpec=inNum.at(9);
            minAreaSpec = atoi(inString.at(8).c_str());
            Mat src=inMat.at(1).clone();//待测外扩图
            Mat golden=inMat.at(2)(inRect.at(3)).clone();//golden图中的器件图部分

            Mat threGolden,threSrc;
            Mat result;
            double minval, maxval = 0.1;
            Point minpoint, maxpoint;

            //判断图像是否为空
            if(golden.empty()||src.empty()){

                if(golden.empty()){
                    writePstFuncLog("Direction: golden is empty",eLog_Debug, pFunc);
                }
                if(src.empty()){
                    writePstFuncLog("Direction: src  is empty",eLog_Debug, pFunc);
                }
                outString->push_back("Direction");
                return VIS_Result_FAIL;
            }

            if(golden.channels()!=3){
                cvtColor(golden.clone(),golden,COLOR_GRAY2BGR);
            }
            if(src.channels()!=3){
                cvtColor(src.clone(),src,COLOR_GRAY2BGR);
            }


            cvtColor(golden.clone(),threGolden,COLOR_BGR2HSV);
            {
                int w = golden.cols;
                int h = golden.rows;
                for (int i = 0; i < h; i++) {
                    uchar* p =threGolden.ptr<uchar>(i);//获取每行指针
                    for (int j = 0; j < w; j++) {

                        int H=*(p+j*3);//HSV间隔排列，现取H的地址---色调：获取针脚饱和度，去除蓝色背景
                        int S=*(p+j*3+1);//HSV间隔排列，现取S的地址---饱和度：获取针脚饱和度，去除黑色背景
                        int V=*(p+j*3+2);//HSV间隔排列，现取V的地址---亮度值
                        if(H>0 && H < 40 && V>100 ){
                            *(p+j*3)=180;
                            *(p+j*3+1)=240;
                            *(p+j*3+2)=240;
                        }
                        else {
                            *(p+j*3)=0;
                            *(p+j*3+1)=0;
                            *(p+j*3+2)=0;
                        }
                    }
                }
            }
            cvtColor(threGolden.clone(),threGolden,COLOR_HSV2BGR);

            cvtColor(src.clone(),threSrc,COLOR_BGR2HSV);
            {
                int w = threSrc.cols;
                int h = threSrc.rows;
                for (int i = 0; i < h; i++) {
                    uchar* p =threSrc.ptr<uchar>(i);//获取每行指针
                    for (int j = 0; j < w; j++) {

                        int H=*(p+j*3);//HSV间隔排列，现取H的地址---色调：获取针脚饱和度，去除蓝色背景
                        int S=*(p+j*3+1);//HSV间隔排列，现取S的地址---饱和度：获取针脚饱和度，去除黑色背景
                        int V=*(p+j*3+2);//HSV间隔排列，现取V的地址---亮度值
                        if(H>0 && H < 40 && V>100 ){
                            *(p+j*3)=180;
                            *(p+j*3+1)=240;
                            *(p+j*3+2)=240;
                        }
                        else {
                            *(p+j*3)=0;
                            *(p+j*3+1)=0;
                            *(p+j*3+2)=0;
                        }
                    }
                }
            }
            cvtColor(threSrc.clone(),threSrc,COLOR_HSV2BGR);

            //模板匹配定位电容位置
            cv::matchTemplate(threSrc.clone(), threGolden.clone(), result, TM_CCOEFF_NORMED);
            minMaxLoc(result, &minval, &maxval, &minpoint, &maxpoint);
            rect=Rect(maxpoint.x,maxpoint.y,golden.cols,golden.rows);

            //根据电容位置确定方向标志检测区
            //沿用：at(5):电容实际角度(黑色区域在 上:0, 下: 90, 左: 180.,右:270)
            //取上边标志区
            if(trueAngle==0){
                if(rect.y-100>=0){
                    rect2=Rect(rect.x+rect.width/6,rect.y-100,rect.width*2/3,200);//
                }
                else {
                    checkResult=false;
                    errorMsg = errorMsg + "&WrongDirection";
                    outString->push_back("Top : Direction flag ROI outrange");
                    writePstFuncLog("Top : Direction flag ROI outrange",eLog_Debug, pFunc);
                }
            }
            //取下边标志区
            if(trueAngle==90){
                if(rect.y+rect.height+100<=src.rows){
                    rect2=Rect(rect.x+rect.width/6,rect.y+rect.height-100,rect.width*2/3,200);//
                }
                else {
                    checkResult=false;
                    errorMsg = errorMsg + "&WrongDirection";
                    outString->push_back("Bottom : Direction flag ROI outrange");
                    writePstFuncLog("Bottom : Direction flag ROI outrange",eLog_Debug, pFunc);
                }
            }
            //取左边标志区
            if(trueAngle==180){
                if(rect.x-100>=0){
                    rect2=Rect(rect.x-100,rect.y+rect.height/6,200,rect.height*2/3);//
                }
                else {
                    checkResult=false;
                    errorMsg = errorMsg + "&WrongDirection";
                    outString->push_back("Left : Direction flag ROI outrange");
                    writePstFuncLog("Left : Direction flag ROI outrange",eLog_Debug, pFunc);
                }
            }
            //取右边标志区
            if(trueAngle==270){
                if(rect.x+rect.width+100<=src.cols){
                    rect2=Rect(rect.x+rect.width-100,rect.y+rect.height/6,200,rect.height*2/3);//
                }
                else {
                    checkResult=false;
                    errorMsg = errorMsg + "&WrongDirection";
                    outString->push_back("Right : Direction flag ROI outrange");
                    writePstFuncLog("Right : Direction flag ROI outrange",eLog_Debug, pFunc);
                }
            }
            if(outString->size()>1){
                //截取极性标志检测区越界

            }
            else{
                //判断检测区内是否有符合的方向标志，暂只卡面积
                Mat DirectROI=src(rect2);
                cvtColor(DirectROI.clone(),DirectROI,COLOR_BGR2HSV);
                int w = DirectROI.cols;
                int h = DirectROI.rows;
                for (int i = 0; i < h; i++) {
                    uchar* p =DirectROI.ptr<uchar>(i);//获取每行指针
                    for (int j = 0; j < w; j++) {

                        int H=*(p+j*3);//HSV间隔排列，现取H的地址---色调：获取针脚饱和度，去除蓝色背景
                        int S=*(p+j*3+1);//HSV间隔排列，现取S的地址---饱和度：获取针脚饱和度，去除黑色背景
                        int V=*(p+j*3+2);//HSV间隔排列，现取V的地址---亮度值
                        if(H>120*3/4 && H < 200*3/4  && V>100){
                            *(p+j*3)=180;
                            *(p+j*3+1)=255;
                            *(p+j*3+2)=255;
                        }
                        else {
                            *(p+j*3)=0;
                            *(p+j*3+1)=0;
                            *(p+j*3+2)=0;
                        }
                    }
                }

                cvtColor(DirectROI.clone(),DirectROI,COLOR_HSV2BGR);

                if(DirectROI.channels()!=1){
                    cvtColor(DirectROI.clone(),DirectROI,COLOR_BGR2GRAY);
                }
                threshold(DirectROI.clone(), DirectROI, 50, 255, THRESH_BINARY);
                vector<vector<Point>> contours;
                findContours(DirectROI, contours, RETR_EXTERNAL, CHAIN_APPROX_NONE);
                int maxArea=0;

                for (uint i=0; i< contours.size();i++) {
                    int area = int(contourArea(contours[i]));
                    if(area >= maxArea){
                        maxArea=area;
                        maxRect=boundingRect(Mat(contours.at(i)));
                    }
                }
                //判断检测区最大轮廓面积是否大于设定值
                if(maxArea >= minAreaSpec){
                    checkResult=true;
                }
                else {
                    checkResult=false;
                    errorMsg = errorMsg + "&WrongDirection";
                    outString->push_back("Direction");
                }
                qstring=QString("CAE flag maxArea=%1,minAreaSpec=%2").arg(maxArea).arg(minAreaSpec);
                writePstFuncLog(qstring,eLog_Debug, pFunc);

            }
        }


    }

    if ( !check0Result && check1 ) //判定缺件时，给outString赋值
    {
        algOffSet = "0.0";
        errorMsg = errorMsg + "&OffSet";
    }

    if ( !check0Result && check2 )
    {
        algAngle = "0.0";
        errorMsg = errorMsg + "&WrongDirection";
    }

    outInt->at(0) = offSetX;
    outInt->at(1) = outOffSet;
    outInt->at(2) = rotateAngle;
    outInt->at(3) = outRotateAngle * 1000;
    outInt->at(4) = pos.x;
    outInt->at(5) = outX;
    outInt->at(6) = pos.y;
    outInt->at(7) = outY;
    outInt->at(8) = inRadius;
    outInt->at(9) = outRadius;


    qdebug<< "algPos.x:"<< algPos.x;
    qdebug<< "algPos.y:"<< algPos.y;
    qdebug<< "xoffset:"<< algPos.x - pos.x;
    qdebug<< "yoffset:"<< algPos.y - pos.y;
    Mat blackMat(inMat.at(1).rows + abs(algPos.y - pos.y) * 2, inMat.at(1).cols + abs(algPos.x - pos.x) * 2, CV_8UC3, Scalar(0, 0, 0));
    MoveToOtherMat(blackMat, inMat.at(1), Rect(abs(algPos.x - pos.x), abs(algPos.y - pos.y), inMat.at(1).cols, inMat.at(1).rows));
    //imshow("heise", blackMat);
    Mat matBack;
    Rect algOffsetRect = Rect(abs(algPos.x - pos.x) + (inMat.at(1).cols / 5 + (algPos.x - pos.x)), abs(algPos.y - pos.y) + (inMat.at(1).rows / 5 + (algPos.y - pos.y)), inMat.at(1).cols * 3 / 5, inMat.at(1).rows * 3 / 5);//加上算法计算的偏移
    if ( algOffsetRect.x < 0 || algOffsetRect.y < 0 || algOffsetRect.x + algOffsetRect.width > blackMat.cols || algOffsetRect.y + algOffsetRect.height > blackMat.rows )
    {
        qdebug<< "???";
        outMat->push_back(matCheck.clone());
    }
    else
    {
        matBack = blackMat(algOffsetRect);
        outMat->push_back(matBack.clone());
    }


    //inMat.at(1)外扩图上画参数
    MoveToOtherMat(inMat.at(1), matCheckClone, rectOffSet);
    //画偏移检测框
    Rect rectExpansion = Rect(inRect.at(1).x + inMat.at(1).cols/5 - offSetX, inRect.at(1).y + inMat.at(1).rows/5 - offSetY, inRect.at(1).width + 2 * offSetX, inRect.at(1).height + 2 * offSetY);
    if ( errorMsg.contains("OffSet") )
    {
        rectangle(inMat.at(1), rectExpansion, Red, 6);
    }
    else
    {
        rectangle(inMat.at(1), rectExpansion, Green, 6);
    }
    //绘制检星形电容方向的辅助框
    if(!normal_cae){
        cv::rectangle(inMat.at(1),rect,Scalar(255,0,0),8,8,0);//绘制器件检测区
        //以不同颜色标记检测结果
        if(errorMsg.contains("Direction")){
            cv::rectangle(inMat.at(1),rect2,Red,8,8,0);//绘制电容方向标志检测区
            cv::rectangle(inMat.at(1),Rect(rect2.x+maxRect.x,rect2.y+maxRect.y,maxRect.width,maxRect.height),Red,8,8,0);//绘制电容标志
        }
        else{
            cv::rectangle(inMat.at(1),rect2,Green,8,8,0);//绘制电容方向标志检测区
            cv::rectangle(inMat.at(1),Rect(rect2.x+maxRect.x,rect2.y+maxRect.y,maxRect.width,maxRect.height),Green,8,8,0);//绘制电容标志
        }
    }

    if(checkResult)
    {
        qdebug<< "all pass";
        rectangle(matFov, rectFov, Scalar(0, 255, 0), 10, 8);
        nzs = QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%17,%18,%19,%20,%21,%22,%23,%24,%25,%26,%27,%28,%29,%30,%31,%32,%33,%34")
                .arg(liaoHao).arg(weiHao).arg(packType).arg(spanFov).arg(algName).arg("PASS")
                .arg(standMissing).arg(algMissing).arg(standOffSet).arg(algOffSet).arg(standAngle).arg(algAngle).arg(-999).arg(-999)
                .arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999)
                .arg(inOCRSpec).arg(outOCRActural).arg(in3DSpec).arg(out3DActural).arg(inAISpec).arg(outAIActural).arg(-999).arg(-999).arg(-999).arg(errorMsg);
        outString->at(0) = nzs.toStdString();
    }
    else
    {
        qdebug<< "fail";
        rectangle(matFov, rectFov, Scalar(0, 0, 255), 10, 8);
        nzs = QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%17,%18,%19,%20,%21,%22,%23,%24,%25,%26,%27,%28,%29,%30,%31,%32,%33,%34")
                .arg(liaoHao).arg(weiHao).arg(packType).arg(spanFov).arg(algName).arg("FAIL")
                .arg(standMissing).arg(algMissing).arg(standOffSet).arg(algOffSet).arg(standAngle).arg(algAngle).arg(-999).arg(-999)
                .arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999).arg(-999)
                .arg(inOCRSpec).arg(outOCRActural).arg(in3DSpec).arg(out3DActural).arg(inAISpec).arg(outAIActural).arg(-999).arg(-999).arg(-999).arg(errorMsg);
        outString->at(0) = nzs.toStdString();
    }

    //新标准需要在outstring的末尾push一个元件相对于蓝色框的偏移，没有填 “0.0”
     outString->emplace_back("0,0");

    endTime = GetTickCount();
    unsigned long useTime = (endTime - startTime);
    if ( pFunc )
    {
        QString quses = "CAE_Inspect use time:" + QString::number(useTime) + " ms";
        pFunc->mDLLLogCatfunc(quses.toStdString(), eLog_Normal);
    }
    return VIS_Result_OK;
}





















