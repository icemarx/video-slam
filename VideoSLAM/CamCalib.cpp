#include "stdafx.h"


using namespace std;
using namespace cv;



void cameraCalibration(vector<vector<Point2f>> imagePoints, Size grid_size, float radius, Mat& cameraMatrix, Mat& distortionCoefficiens) {
    vector<vector<Point3f>> worldPoints(1);
    for(int i = 0; i < grid_size.height; i++) {
        for(int j = 0; j < grid_size.width; j++) {
            worldPoints[0].push_back(Point3f(j * radius, i * radius, 0.0f));
        }
    }
    worldPoints.resize(imagePoints.size(), worldPoints[0]);

    vector<Mat> rvecs, tvecs;
    calibrateCamera(worldPoints, imagePoints, grid_size, cameraMatrix, distortionCoefficiens, rvecs, tvecs);
}

double calibrateAndReproject(vector<vector<Point2f>> imagePoints, Size grid_size, float radius, Mat& cameraMatrix, Mat& distortionCoefficiens) {
    vector<double> reprojectionErrors;
    vector<Point3f> newObjectPoints;

    vector<vector<Point3f>> worldPoints(1);
    for(int i = 0; i < grid_size.height; i++) {
        for(int j = 0; j < grid_size.width; j++) {
            worldPoints[0].push_back(Point3f(j * radius, i * radius, 0));
        }
    }
    worldPoints[0][grid_size.width - 1].x = worldPoints[0][0].x + radius * (grid_size.width - 1);
    newObjectPoints = worldPoints[0];

    worldPoints.resize(imagePoints.size(), worldPoints[0]);

    // find intrinsic and extrinsic camera parameters
    int iFixedPoint = -1;
    vector<Mat> rvecs, tvecs;
    double rms = calibrateCameraRO(worldPoints, imagePoints, grid_size, iFixedPoint, cameraMatrix, distortionCoefficiens, rvecs, tvecs, newObjectPoints, CALIB_USE_LU);
    cout << "reprojection error reported by calibrateCameraRO: " << rms << endl;
    
    // compute average error:
    worldPoints.clear();
    worldPoints.resize(imagePoints.size(), newObjectPoints);
    
    vector<Point2f> imagePoints2;
    size_t totalPoints = 0;
    double totalError = 0, err;

    for(int i = 0; i < worldPoints.size(); ++i) {
        projectPoints(worldPoints[i], rvecs[i], tvecs[i], cameraMatrix, distortionCoefficiens, imagePoints2);
        err = norm(imagePoints[i], imagePoints2, NORM_L2);

        size_t n = worldPoints[i].size();
        cout << "Per view error: " << i << ": " << err * err / n << endl;
        totalError += err * err;
        totalPoints += n;
    }

    double average_error = std::sqrt(totalError / totalPoints);
    cout << "average error: " << average_error << endl;
    return average_error;
}


int CamCalib::myCalibrateCamera(string fileName, bool preview) {
    int limit = 130;                     // limit the number of images needed
    double max_reprojection_error = 0.01;  // threshold at which the calibration is good enough    TODO: find proper threshold
    int skip = 4;
    bool first = true;
    int num_images = 347;               // number of images used for calibration
    Size cal_grid_size(7,5);            // num cols, num rows; on the pattern used for calibration
    float cal_dot_r = 0.006f;           // single dot on grid radius in m
    float cal_square_size = 0.015f;     // side of a square on grid, generated by 4 grid points
    vector<vector<Point2f>> imagePoints;
    String window_name = "Calibration Preview";

    
    Mat cameraMatrix = Mat::eye(3, 3, CV_64F);
    Mat distortionCoefficients = Mat::zeros(8, 1, CV_64F);
    

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

            count++;
            skip = (skip + 1) % 5;
            if(skip != 0) continue;

            imagePoints.push_back(pointBuf);
            
            if(first) {
                first = false;
                continue;
            }

            
            // calibrate
            cameraMatrix = Mat::eye(3, 3, CV_64F);
            distortionCoefficients = Mat::zeros(8, 1, CV_64F);
            double reprojectionError = calibrateAndReproject(imagePoints, cal_grid_size, cal_dot_r, cameraMatrix, distortionCoefficients);
            reprojectionError = calibrateAndReproject(imagePoints, cal_grid_size, cal_square_size, cameraMatrix, distortionCoefficients);
            // cout << "Current reprojectionError: " << reprojectionError << endl;
            

            if(preview) {
                // display and save image
                drawChessboardCorners(cal_image, cal_grid_size, Mat(pointBuf), found);
                string file_name = format("UsedCalibrationImages/Saved_%03d.png", img_num);
                bool created = imwrite(file_name, cal_image);
                cout << file_name << " created: " << created << endl;
                // cout << "Current reprojection error: " << reprojectionError << endl;
            }

            // if(reprojectionError <= )
        }

        // display image
        if(preview)imshow(window_name, cal_image);

        // break out of the loop for faster testing
        if(count >= limit) break;
    }

    
    // compute camera matrix and distortion coefficients
    /*
    cout << "Calibrating" << endl;
    Mat cameraMatrix = Mat::eye(3, 3, CV_64F);
    Mat distortionCoefficients = Mat::zeros(8, 1, CV_64F);
    cameraCalibration(imagePoints, cal_grid_size, cal_dot_r, cameraMatrix, distortionCoefficients);
    cameraCalibration(imagePoints, cal_grid_size, cal_square_size, cameraMatrix, distortionCoefficients);
    */

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

        // save distortion coefficients
        for(int r = 0; r < distortionCoefficients.rows; r++) {
            double value = distortionCoefficients.at<double>(r, 0);
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
