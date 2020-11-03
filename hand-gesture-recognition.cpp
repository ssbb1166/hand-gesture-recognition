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

	// �ָ��� �� (�հ��� ���� 0��) �׸���
	if (finger_count == 0) {
		circle(drawing_mask, Point(center.x, center.y), 10, Scalar(0), -1);
		cout << "Draw" << endl;
	}
	// ������ �� (�հ��� ���� 2��) �����
	if (finger_count == 2) {
		circle(drawing_mask, Point(center.x, center.y), 20, Scalar(255), -1);
		cout << "Erase" << endl;
	}

	// �׸��� �׸���.
	tmp = ROI.clone();
	ROI = Scalar::all(0);
	tmp.copyTo(ROI, drawing_mask);
}


void printText(Mat& ROI, Point center) {
	// �ؽ�Ʈ ���
	string count = "finger:  ";
	int fontFace = FONT_HERSHEY_SCRIPT_SIMPLEX;
	double fontScale = 0.8;
	int thickness = 2;
	int baseline = 0;
	Size textSize = getTextSize(count, fontFace, fontScale, thickness, &baseline);

	// �ؽ�Ʈ ���͸� �����Ѵ�.
	Point textOrg1((center.x - textSize.width / 2.0), (center.y - textSize.height * 2.0));
	Point textOrg2((center.x - textSize.width / 2.0), (center.y - textSize.height * 4.0));

	// �ؽ�Ʈ�� ����Ѵ�.
	putText(ROI, "(" + to_string(center.x) + "," + to_string(center.y) + ")", textOrg1, fontFace, fontScale, Scalar(0, 255, 0), thickness, 8);
	putText(ROI, count + to_string(finger_count), textOrg2, fontFace, fontScale, Scalar(0, 0, 255), thickness, 8);
}


Point getHandCenter(const Mat& mask) {
	Mat dst;  // �Ÿ� ��ȯ ����� ������ ����
	int maxIdx[2];  // ��ǥ ���� ���� �迭 (��, �� ������ ����)

	// �Ÿ� ��ȯ ����� dst�� �����Ѵ�. (���: CV_32SC1 Ÿ��)
	distanceTransform(mask, dst, DIST_L2, 5);

	// �Ÿ� ��ȯ ��Ŀ��� ��(�Ÿ�)�� ���� ū �ȼ��� ��ǥ�� ���� ���´�. (�ִ񰪸� ���)
	minMaxIdx(dst, NULL, &radius, NULL, maxIdx, mask);

	return Point(maxIdx[1], maxIdx[0]);
}


void getFingerCount(const Mat& mask, Point center, double radius, double scale) {
	// �հ��� ������ ���� ���� ���� �׸���.
	Mat cImg(mask.size(), CV_8U, Scalar(0));
	circle(cImg, center, radius * scale, Scalar(255));

	// ���� �ܰ����� ã�´�.
	vector<vector<Point>> contours;
	findContours(cImg, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

	// �ܰ����� ������ ���� �������� �ʴ´�.
	if (contours.size() == 0)
		return;

	// �ܰ����� ���� ���� mask�� ���� 0���� 1�� �ٲ�� ������ ����.
	int count = 0;
	for (int i = 1; i < contours[0].size(); i++) {
		Point p1 = contours[0][i - 1];
		Point p2 = contours[0][i];
		if (mask.at<uchar>(p1.y, p1.x) == 0 && mask.at<uchar>(p2.y, p2.x) > 1)
			count++;
	}

	// �ո�� ������ ���� 1���� �����Ѵ�.
	finger_count = count - 1;
	if (finger_count < 0)
		finger_count = 0;

	// �հ��� ������ ����Ѵ�.
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

	// ī�޶� ����.
	cap.open(deviceID + apiID);

	// ī�޶� ��ȿ���� Ȯ���Ѵ�.
	if (!cap.isOpened()) {
		cerr << "ERROR! Unable to open camera\n";
		return -1;
	}

	// ROI ������ �׸��� �׸��� ���� ����ũ�� �����Ѵ�.
	cap.read(frame);
	drawing_mask = Mat(frame.rows, frame.cols / 2, CV_8UC1, Scalar(255));

	while (true) {
		// ����̽��κ��� ������ �о� frame�� �ִ´�.
		cap.read(frame);

		// ������ ��ȿ���� Ȯ���Ѵ�.
		if (frame.empty()) {
			cerr << "ERROR! blank frame grabbed\n";
			break;
		}

		// ���� ������ ROI ������ �����Ѵ�.
		part.x = part.y = 0;
		part.height = frame.rows;
		part.width = frame.cols / 2;
		ROI = frame(part);
		rectangle(frame, part, Scalar(0, 0, 255), 1);

		// Hand Gesture Recognition --------------------

		// ROI ������ YCrCb �������� ��ȯ�Ѵ�.
		cvtColor(ROI, ycrcb, COLOR_BGR2YCrCb);
		split(ycrcb, channels);

		// �Ǻ� ������ �����Ѵ�. (133 <= Cr <= 173, 77 <= Cb <= 127)
		inRange(ycrcb, Scalar(0, 133, 77), Scalar(255, 173, 127), skin_area);

		// ���� ������ �����Ѵ�.
		Mat element9(9, 9, CV_8U, Scalar(1));
		morphologyEx(skin_area, skin_area, MORPH_OPEN, element9);

		// ���� �߽��� ã�´�.
		center = getHandCenter(skin_area);
		circle(ROI, center, 2, Scalar(0, 255, 0), -1);
		circle(ROI, center, (int)(radius * 2.0), Scalar(255, 0, 0), 2);

		// �հ��� ������ ���Ѵ�.
		getFingerCount(skin_area, center, radius, 2.0);

		// ���콺 �����Ͱ� ���� �߽��� �����Ѵ�.
		namedWindow("Live", WINDOW_NORMAL);
		HWND hWnd = FindWindow(NULL, L"Live");
		mouse_pointer.x = center.x;
		mouse_pointer.y = center.y;
		ClientToScreen(hWnd, &mouse_pointer);
		SetCursorPos(mouse_pointer.x, mouse_pointer.y);

		// ���� �̿��ؼ� �׸��� �׸���.
		drawPoint();

		// ������ �ǽð����� ����Ѵ�.
		imshow("Live", frame);

		if (waitKey(50) >= 0)
			break;
	}

	return 0;
}
