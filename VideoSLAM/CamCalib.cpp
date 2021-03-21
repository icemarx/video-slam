#include "stdafx.h"
#include "CamCalib.h"

#include <iostream>
#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>
#include <opencv2/core/quaternion.hpp>

using namespace std;
using namespace cv;

int CamCalib::myCalibrateCamera() {
    // TODO set these to accurate 
    int num_images = 347;        // number of images used for calibration
    Size cal_grid_size(7,5);   // num cols, num rows; on the pattern used for calibration
    double cal_dot_r = 0.006;   // single dot on grid radius in mm
    vector<vector<Point2f>> imagePoints;

    String window_name = "Calibration Preview";
    namedWindow(window_name, WINDOW_NORMAL);

    int count = 0;
    for(int img_num = 0; img_num < num_images; img_num++) {
        // open image files
        string file_name = format("Calibration/CAL_IMG (%d).jpg", img_num);
        Mat cal_image = imread(file_name);

        // pre-process image
        // threshold(cal_image, cal_image, 127, 255, THRESH_OTSU);

        // flip image
        // flip(cal_image, cal_image, 0);

        // find pattern
        vector<Point2f> pointBuf;
        bool found = findCirclesGrid(cal_image, cal_grid_size, pointBuf);

        if(found) {
            imagePoints.push_back(pointBuf);
            cout << "Found circle grid, IMG: " << img_num << endl;

            drawChessboardCorners(cal_image, cal_grid_size, Mat(pointBuf), found);
            count++;
        }/* else {
            cout << "Not found :(" << endl;
        }*/

        /*
        // display image
        imshow(window_name, cal_image);
        char key = (char) waitKey(1);
        if(key == 27) break;
        */
    }

    // TODO: runCalibrationAndSave
    return count;
}

CamCalib::CamCalib() {}
CamCalib::~CamCalib() = default;
