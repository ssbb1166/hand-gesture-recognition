#include <iostream>
#include <string>
#include <windows.h>
#include <opencv2/opencv.hpp>
#include <opencv2/ximgproc.hpp>
#include <opencv2/highgui.hpp>


using namespace std;
using namespace cv;


Mat frame, ROI, drawing_mask;
POINT mouse_pointer;
Point center;
Rect part(0, 0, 0, 0);
int finger_count;
double radius;


void drawPoint() {
	Mat tmp;

	// 주먹일 때 (손가락 개수 0개) 그리기
	if (finger_count == 0) {
		circle(drawing_mask, Point(center.x, center.y), 10, Scalar(0), -1);
		cout << "Draw" << endl;
	}
	// 가위일 때 (손가락 개수 2개) 지우기
	if (finger_count == 2) {
		circle(drawing_mask, Point(center.x, center.y), 20, Scalar(255), -1);
		cout << "Erase" << endl;
	}

	// 그림을 그린다.
	tmp = ROI.clone();
	ROI = Scalar::all(0);
	tmp.copyTo(ROI, drawing_mask);
}


void printText(Mat& ROI, Point center) {
	// 텍스트 요소
	string count = "finger:  ";
	int fontFace = FONT_HERSHEY_SCRIPT_SIMPLEX;
	double fontScale = 0.8;
	int thickness = 2;
	int baseline = 0;
	Size textSize = getTextSize(count, fontFace, fontScale, thickness, &baseline);

	// 텍스트 센터를 지정한다.
	Point textOrg1((center.x - textSize.width / 2.0), (center.y - textSize.height * 2.0));
	Point textOrg2((center.x - textSize.width / 2.0), (center.y - textSize.height * 4.0));

	// 텍스트를 출력한다.
	putText(ROI, "(" + to_string(center.x) + "," + to_string(center.y) + ")", textOrg1, fontFace, fontScale, Scalar(0, 255, 0), thickness, 8);
	putText(ROI, count + to_string(finger_count), textOrg2, fontFace, fontScale, Scalar(0, 0, 255), thickness, 8);
}


Point getHandCenter(const Mat& mask) {
	Mat dst;  // 거리 변환 행렬을 저장할 변수
	int maxIdx[2];  // 좌표 값을 얻어올 배열 (행, 열 순으로 저장)

	// 거리 변환 행렬을 dst에 저장한다. (결과: CV_32SC1 타입)
	distanceTransform(mask, dst, DIST_L2, 5);

	// 거리 변환 행렬에서 값(거리)이 가장 큰 픽셀의 좌표와 값을 얻어온다. (최댓값만 사용)
	minMaxIdx(dst, NULL, &radius, NULL, maxIdx, mask);

	return Point(maxIdx[1], maxIdx[0]);
}


void getFingerCount(const Mat& mask, Point center, double radius, double scale) {
	// 손가락 개수를 세기 위한 원을 그린다.
	Mat cImg(mask.size(), CV_8U, Scalar(0));
	circle(cImg, center, radius * scale, Scalar(255));

	// 원의 외곽선을 찾는다.
	vector<vector<Point>> contours;
	findContours(cImg, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

	// 외곽선이 없으면 손을 검출하지 않는다.
	if (contours.size() == 0)
		return;

	// 외곽선을 따라 돌며 mask의 값이 0에서 1로 바뀌는 지점을 센다.
	int count = 0;
	for (int i = 1; i < contours[0].size(); i++) {
		Point p1 = contours[0][i - 1];
		Point p2 = contours[0][i];
		if (mask.at<uchar>(p1.y, p1.x) == 0 && mask.at<uchar>(p2.y, p2.x) > 1)
			count++;
	}

	// 손목과 만나는 지점 1개를 제외한다.
	finger_count = count - 1;
	if (finger_count < 0)
		finger_count = 0;

	// 손가락 개수를 출력한다.
	cout << "finger count: " << finger_count << endl;
	printText(ROI, center);
}


int main(int argc, char** argv)
{
	VideoCapture cap;
	Mat ycrcb, channels[3], skin_area;

	// Advance usage: select any API backend
	int deviceID = 0;     // 0 = Open default camera
	int apiID = CAP_ANY;  // 0 = Autodetect default API

	// 카메라를 연다.
	cap.open(deviceID + apiID);

	// 카메라가 유효한지 확인한다.
	if (!cap.isOpened()) {
		cerr << "ERROR! Unable to open camera\n";
		return -1;
	}

	// ROI 영역에 그림을 그리기 위한 마스크를 설정한다.
	cap.read(frame);
	drawing_mask = Mat(frame.rows, frame.cols / 2, CV_8UC1, Scalar(255));

	while (true) {
		// 디바이스로부터 영상을 읽어 frame에 넣는다.
		cap.read(frame);

		// 영상이 유효한지 확인한다.
		if (frame.empty()) {
			cerr << "ERROR! blank frame grabbed\n";
			break;
		}

		// 손을 검출할 ROI 영역을 지정한다.
		part.x = part.y = 0;
		part.height = frame.rows;
		part.width = frame.cols / 2;
		ROI = frame(part);
		rectangle(frame, part, Scalar(0, 0, 255), 1);

		// Hand Gesture Recognition --------------------

		// ROI 영역을 YCrCb 영상으로 변환한다.
		cvtColor(ROI, ycrcb, COLOR_BGR2YCrCb);
		split(ycrcb, channels);

		// 피부 영역을 검출한다. (133 <= Cr <= 173, 77 <= Cb <= 127)
		inRange(ycrcb, Scalar(0, 133, 77), Scalar(255, 173, 127), skin_area);

		// 열림 연산을 적용한다.
		Mat element9(9, 9, CV_8U, Scalar(1));
		morphologyEx(skin_area, skin_area, MORPH_OPEN, element9);

		// 손의 중심을 찾는다.
		center = getHandCenter(skin_area);
		circle(ROI, center, 2, Scalar(0, 255, 0), -1);
		circle(ROI, center, (int)(radius * 2.0), Scalar(255, 0, 0), 2);

		// 손가락 개수를 구한다.
		getFingerCount(skin_area, center, radius, 2.0);

		// 마우스 포인터가 손의 중심을 추적한다.
		namedWindow("Live", WINDOW_NORMAL);
		HWND hWnd = FindWindow(NULL, L"Live");
		mouse_pointer.x = center.x;
		mouse_pointer.y = center.y;
		ClientToScreen(hWnd, &mouse_pointer);
		SetCursorPos(mouse_pointer.x, mouse_pointer.y);

		// 손을 이용해서 그림을 그린다.
		drawPoint();

		// 영상을 실시간으로 출력한다.
		imshow("Live", frame);

		if (waitKey(50) >= 0)
			break;
	}

	return 0;
}
