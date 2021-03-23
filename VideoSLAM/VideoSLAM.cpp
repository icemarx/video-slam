// VideoSLAM.cpp : Defines the entry point for the console application.

#include "stdafx.h"

using namespace std;
using namespace cv;

// constants
const Ptr<cv::aruco::Dictionary> dictionary = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_4X4_50);
static const int WORLD = -2;
static const int INIT_MIN_SIZE_VALUE = 1000000;

// functions
/// <sumary>
/// Generates the image files of aruco markers. The generated markers should be used in the input video,
/// although any markers in the designated dictionary should work.
/// <see cref="dictionary"/>
/// </sumary>
/// <param name="num_of_markers">The number of markers that will be generated. Max is based on dictionary used</param>
void generateArucoMarkers(int num_of_markers) {
    for(int i = 0; i < num_of_markers; i++) {
        Mat markerImage;
        aruco::drawMarker(dictionary, i, 200, markerImage, 1);

        string file_name = format("MarkerImages/Marker_%03d.png", i);
        bool created = imwrite(file_name, markerImage);
        cout << file_name << " created: " << created << endl;
    }
}

/// <sumary>
/// Computes the camera pose in regards to the detected rotation and translation vectors of marker and writes it
/// to MarkerInfo.
/// </sumary>
/// <param name="marker">MarkerInfo of the marker, to which to write the results</param>
/// <param name="r_vec">Rotation vector of the marker.</param>
/// <param name="t_vec">Translation vector of the marker</param>
void getCameraPoseBasedOnMarker(MarkerInfo& marker, Vec3d r_vec, Vec3d t_vec) {
    Mat marker_rotation(3, 3, CV_64FC1);
    Mat marker_translation = Mat(t_vec);
    Rodrigues(r_vec, marker_rotation);
    // auto marker_translation = tvecs[i];

    Mat rotate_to_os(3, 3, CV_64FC1);
    rotate_to_os.at<double>(0, 0) = -1.0;
    rotate_to_os.at<double>(0, 1) = 0;
    rotate_to_os.at<double>(0, 2) = 0;
    rotate_to_os.at<double>(1, 0) = 0;
    rotate_to_os.at<double>(1, 1) = 0;
    rotate_to_os.at<double>(1, 2) = 1.0;
    rotate_to_os.at<double>(2, 0) = 0.0;
    rotate_to_os.at<double>(2, 1) = 1.0;
    rotate_to_os.at<double>(2, 2) = 0.0;

    marker_rotation = marker_rotation * rotate_to_os.t();
    
    // set to MarkerInfo
    marker.current_camera_pose_position_x = marker_translation.at<double>(0, 0);
    marker.current_camera_pose_position_y = marker_translation.at<double>(1, 0);
    marker.current_camera_pose_position_z = marker_translation.at<double>(2, 0);
    /*
    marker.current_camera_pose_orientation_x = quat.x;
    marker.current_camera_pose_orientation_y = quat.y;
    marker.current_camera_pose_orientation_z = quat.z;
    marker.current_camera_pose_orientation_w = quat.w;
    */
    marker.current_camera_pose_position = marker_translation;
    marker.current_camera_pose_orientation = Quat<double>::createFromRotMat(marker_rotation);
}

/// <sumary>
/// Performs a transform inversion on the marker's camera_pose_position and orientation.
/// </sumary>
/// <param name="marker">MarkerInfo of the marker, on which the camera_pose will be inverted</param>
void markerPoseInverse(MarkerInfo& marker) {
    // rotation inverse
    marker.current_camera_pose_orientation = inv(marker.current_camera_pose_orientation);

    // translation inverse
    Mat marker_translation = marker.current_camera_pose_orientation.toRotMat3x3() * -marker.current_camera_pose_position;
    /*marker_translation = *(new Vec3f(temp_matrix.at<float>(0, 0),
        temp_matrix.at<float>(1, 0),
        temp_matrix.at<float>(2, 0)));*/
 
    // set to MarkerInfo
    marker.current_camera_pose_position_x = marker_translation.at<double>(0, 0);
    marker.current_camera_pose_position_y = marker_translation.at<double>(1, 0);
    marker.current_camera_pose_position_z = marker_translation.at<double>(2, 0);


    /*
    marker.current_camera_pose_orientation_x = quat.x;
    marker.current_camera_pose_orientation_y = quat.y;
    marker.current_camera_pose_orientation_z = quat.z;
    marker.current_camera_pose_orientation_w = quat.w;
    */
}

void readCalibrationData(ifstream& inputFile, Mat& cameraMatrix, Mat& distCoeff) {
    cameraMatrix = Mat::eye(3, 3, CV_64F);
    distCoeff = Mat::zeros(5, 1, CV_64F);

    // camera matrix
    for(int r = 0; r < cameraMatrix.rows; r++) {
        for(int c = 0; c < cameraMatrix.cols; c++) {
            inputFile >> cameraMatrix.at<double>(r, c);
        }
    }

    // distance coefficients
    for(int r = 0; r < distCoeff.rows; r++) {
        inputFile >> distCoeff.at<double>(r, 0);
    }
}


int main() {
    // Prepare fixed information
    int num_of_markers = 12;
    double marker_length = 0.075; // meters
    vector<MarkerInfo> markers(num_of_markers);
    vector<vector<Point2f>> corners, rejects;
    Ptr<aruco::DetectorParameters> parameters = aruco::DetectorParameters::create();

    bool first_marker_detected = false;
    int lowest_marker_id = -1;

    // Generate Marker Images
    generateArucoMarkers(num_of_markers);

    // Check for camera calibration
    string calibration_data_file_name = "Calibration/calibration";
    ifstream inputFile;
    inputFile.open(calibration_data_file_name);
    if(inputFile.fail()) {
        // no calibration file has been found
        cerr << "Could not open calibration file" << endl;

        // calibrate
        cout << "Initiating calibration:" << endl;
        CamCalib cameraCalibration;
        cameraCalibration.myCalibrateCamera(calibration_data_file_name, false);

        // try reading again
        inputFile.open(calibration_data_file_name);
        if(inputFile.fail()) {
            cerr << "Could not open calibration file after calibration" << endl;
            cerr << "Closing application" << endl;
            return 3;
        }
    }

    // read the data
    Mat cameraMatrix = Mat::eye(3, 3, CV_64F);
    Mat distCoeff = Mat::zeros(5, 1, CV_64F);
    readCalibrationData(inputFile, cameraMatrix, distCoeff);
    inputFile.close();
    
    cout << cameraMatrix << endl;
    cout << distCoeff << endl;

    // Find and open the video
    // VideoCapture cap("InputVideos/Input_1.mp4");
    // VideoCapture cap("InputVideos/Input_2.mp4");
    VideoCapture cap("InputVideos/Input_3.mp4");
    if(!cap.isOpened()) {
        cout << "Cannot open the video file" << endl;
        cin.get();
        return -1;
    }

    double fps = cap.get(CAP_PROP_FPS);
    std::cout << "Frames per seconds : " << fps << endl;

    // Visualization code
    String window_name = "Video Preview";
    namedWindow(window_name, WINDOW_NORMAL);

    // Work on the video frame-by-frame
    Mat frame;
    int marker_counter = 0;
    for(int frame_num = 0; cap.read(frame); frame_num++) {

        // set frame specific variables
        vector<MarkerInfo> temp_markers;
        int marker_counter_previous = marker_counter;
        for(int i = 0; i < num_of_markers; i++)
            markers[i].visible = false;

        // detect markers in this frame
        vector<int> detected_IDs;
        aruco::detectMarkers(frame, dictionary, corners, detected_IDs, parameters, rejects);

        // marker pose estimation
        vector<Vec3d> rvecs, tvecs;
        aruco::estimatePoseSingleMarkers(corners, marker_length, cameraMatrix, distCoeff, rvecs, tvecs);


        // Visualization code
        aruco::drawDetectedMarkers(frame, corners, detected_IDs);
        // cout << marker_IDs.size() << endl;
        for(int i = 0; i < detected_IDs.size(); i++) {
            cout << detected_IDs[i] << endl;

            aruco::drawAxis(frame, cameraMatrix, distCoeff, rvecs[i], tvecs[i], 0.1);
        }
        imshow(window_name, frame);
        cout << endl;
        // end visualization code

        if(waitKey(10) == 27) {
            cout << "ESC pressed" << endl;
            break;
        }


        ///////////////////////////////////////////////
        // SLAM
        ///////////////////////////////////////////////
        size_t num_detected = detected_IDs.size();
        if(num_detected <= 0) {
            cout << "No Markers detected" << endl;
            continue;
        }
        // markers are detected


        // handle first detected marker
        if(!first_marker_detected) {
            first_marker_detected = true;

            // detect the marker with lowest ID
            lowest_marker_id = detected_IDs[0];
            for(int i = 1; i < num_detected; i++) {
                if(detected_IDs[i] < lowest_marker_id)
                    lowest_marker_id = detected_IDs[i];
            }

            // set it to world's origin
            markers[0].marker_id = lowest_marker_id;

            markers[0].world_position_x = 0;
            markers[0].world_position_y = 0;
            markers[0].world_position_z = 0;
            markers[0].world_orientation_x = 0;
            markers[0].world_orientation_y = 0;
            markers[0].world_orientation_z = 0;
            markers[0].world_orientation_w = 1;

            // relative position equals Global position
            markers[0].to_prev_position_x = 0;
            markers[0].to_prev_position_y = 0;
            markers[0].to_prev_position_z = 0;
            markers[0].to_prev_orientation_x = 0;
            markers[0].to_prev_orientation_y = 0;
            markers[0].to_prev_orientation_z = 0;
            markers[0].to_prev_orientation_w = 1;

            // increase count
            marker_counter++;
            
            // visibility
            markers[0].visible = true;

            // previous marker
            markers[0].previous_marker_id = WORLD;

            cout << "Lowest Id marker: " << lowest_marker_id << endl;
        }

        // change visibility flag of detected old markers
        for(int j = 0; j < marker_counter; j++) {
            for(int k = 0; k < num_detected; k++) {
                if(markers[j].marker_id == detected_IDs[k])
                    markers[j].visible = true;
            }
        }

        // for every marker do:
        for(int i = 0; i < num_detected; i++) {
            int index;
            int current_id = detected_IDs[i];

            // NOTE: could draw here

            // check wether marker exists or not
            bool existing = false;
            for(int temp_counter = 0; !existing && temp_counter < marker_counter; temp_counter++) {
                if(markers[temp_counter].marker_id == current_id) {
                    index = temp_counter;
                    existing = true;
                    cout << "Found existing marker with ID: " << current_id << endl;
                }
            }

            if(!existing) {
                // new marker detected
                index = marker_counter;
                markers[index].marker_id = current_id;
                markers[index].visible = true;
                existing = true;
                cout << "Detected new marker with ID: " << current_id << endl;

                getCameraPoseBasedOnMarker(markers[index], rvecs[i], tvecs[i]);

                // test if position can be calculated based on old known marker
                bool any_known_marker_visible = false;
                int last_marker_id;
                for(int k = 0; k < index && !any_known_marker_visible; k++) {
                    if(markers[k].visible == true && markers[k].previous_marker_id != -1) {
                        any_known_marker_visible = true;
                        markers[index].previous_marker_id = k;
                        last_marker_id = k;
                    }
                }

                if(any_known_marker_visible) {
                    // TODO: continue from 397
                    // NOTE: compute transform between two markers without calling a function?
                    // until 460

                    marker_counter++;
                    markerPoseInverse(markers[index]);
                }

            } else {
                // old marker detected
                getCameraPoseBasedOnMarker(markers[index], rvecs[i], tvecs[i]);
                markerPoseInverse(markers[index]);
            }

            // compute global position of a new marker
            if(marker_counter_previous < marker_counter) {  // NOTE: this might be problematic or cause unnecesarry computations
                // TODO: calculate geometry to world transform
                // TODO: save the world transform to MarkerInfo
            }


            // TODO: remove
            cout << "Marker: " << index << endl;
            cout << markers[index].current_camera_pose_position << endl;
            cout << markers[index].current_camera_pose_orientation << endl;
        }

        // compute which of the visible markers is the closest to the camera
        double minimal_distance = INIT_MIN_SIZE_VALUE;
        int closest_index = -1;
        for(int k = 0; k < num_of_markers; k++) {

            // check if marker is visible
            if(markers[k].visible) {
                double a = markers[k].current_camera_pose_position_x;
                double b = markers[k].current_camera_pose_position_y;
                double c = markers[k].current_camera_pose_position_z;
                double size = sqrt(a*a + b * b + c * c);
                
                if(size < minimal_distance) {
                    minimal_distance = size;
                    closest_index = k;
                }
            }
        }

        // compute global camera pose
        // TODO

        // TODO: fix output
        cout << "Frame: " << frame_num << endl;
        cout << "Camera pose based on index: " <<  closest_index << endl;
        cout << "t: " << markers[closest_index].current_camera_pose_position << endl;
        cout << "r: " << markers[closest_index].current_camera_pose_orientation << endl;

    }

    return 0;
}

