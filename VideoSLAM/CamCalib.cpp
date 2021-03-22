#include "stdafx.h"


using namespace std;
using namespace cv;



void cameraCalibration(vector<vector<Point2f>> imagePoints, Size grid_size, float radius, Mat& cameraMatrix, Mat& distanceCoefficiens) {
    // TODO: check these lines
    vector<vector<Point3f>> worldPoints(1);
    for(int i = 0; i < grid_size.height; i++) {
        for(int j = 0; j < grid_size.width; j++) {
            worldPoints[0].push_back(Point3f(j * radius, i * radius, 0.0f));
        }
    }
    worldPoints.resize(imagePoints.size(), worldPoints[0]);

    vector<Mat> rvecs, tvecs;
    calibrateCamera(worldPoints, imagePoints, grid_size, cameraMatrix, distanceCoefficiens, rvecs, tvecs);
}

int CamCalib::myCalibrateCamera(string fileName, bool preview) {
    int limit = 20;                 // limit the number of images needed
    int num_images = 347;           // number of images used for calibration
    Size cal_grid_size(7,5);        // num cols, num rows; on the pattern used for calibration
    double cal_dot_r = 0.006;       // single dot on grid radius in mm
    vector<vector<Point2f>> imagePoints;
    String window_name = "Calibration Preview";

    if(preview) {
        namedWindow(window_name, WINDOW_NORMAL);
    }

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
            cout << "Found circle grid, IMG: " << img_num << endl;
            
            imagePoints.push_back(pointBuf);
            count++;

            if(preview) {
                // display and save image
                drawChessboardCorners(cal_image, cal_grid_size, Mat(pointBuf), found);
                string file_name = format("UsedCalibrationImages/Saved_%03d.png", img_num);
                bool created = imwrite(file_name, cal_image);
                cout << file_name << " created: " << created << endl;
            }
        }

        // display image
        if(preview)imshow(window_name, cal_image);

        // break out of the loop for faster testing
        if(count >= limit) break;
    }

    // compute camera matrix and distance coefficients
    cout << "Calibrating" << endl;
    Mat cameraMatrix = Mat::eye(3, 3, CV_64F);
    Mat distanceCoefficients = Mat::zeros(8, 1, CV_64F);
    cameraCalibration(imagePoints, cal_grid_size, cal_dot_r, cameraMatrix, distanceCoefficients);

    // save matrices
    ofstream outStream(fileName);
    if(outStream) {

        // save camera matrix
        for(int r = 0; r < cameraMatrix.rows; r++) {
            for(int c = 0; c < cameraMatrix.cols; c++) {
                double value = cameraMatrix.at<double>(r, c);
                outStream << value << endl;
            }
        }

        // save distance coefficients
        for(int r = 0; r < distanceCoefficients.rows; r++) {
            double value = distanceCoefficients.at<double>(r, 0);
            outStream << value << endl;
        }

        outStream.close();
        cout << "Calibration saved" << endl;
    } else {
        cout << "Could not write to calibration file" << endl;
    }

    return count;
}

CamCalib::CamCalib() {}
CamCalib::~CamCalib() = default;
