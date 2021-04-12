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
    Rodrigues(r_vec, marker_rotation);

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

    // cout << Quat<double>::createFromRotMat(marker_rotation) << endl;
    // cout << Quat<double>::createFromRotMat(rotate_to_os.t()) << endl;
    // cout << Quat<double>::createFromRotMat(marker_rotation) * Quat<double>::createFromRotMat(rotate_to_os.t()) << endl;
    marker_rotation = marker_rotation * rotate_to_os.t();
    // cout << Quat<double>::createFromRotMat(rotate_to_os).x << endl;
    // cout << Quat<double>::createFromRotMat(rotate_to_os).y << endl;
    // cout << Quat<double>::createFromRotMat(rotate_to_os).z << endl;
    // cout << Quat<double>::createFromRotMat(rotate_to_os).w << endl;
    
    // set to MarkerInfo
    marker.current_camera_pose_position = t_vec;
    marker.current_camera_pose_orientation = Quat<double>::createFromRotMat(marker_rotation);
}

/// <sumary>
/// Performs a transform inversion on the marker's camera_pose_position and orientation.
/// </sumary>
/// <param name="marker">MarkerInfo of the marker, on which the camera_pose will be inverted</param>
void markerPoseInverse(MarkerInfo& marker) {
    // rotation inverse
    // TODO: fix
    // marker.current_camera_pose_orientation = inv(marker.current_camera_pose_orientation);

    // translation inverse
    // auto marker_translation = marker.current_camera_pose_orientation.toRotMat3x3() * -marker.current_camera_pose_position;
    // TODO: check if correct, although this should work in world coordinates
    auto marker_translation = -marker.current_camera_pose_position;

    // set to MarkerInfo
    marker.current_camera_pose_position = marker_translation;
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

void computeTransforms(MarkerInfo& current_marker, MarkerInfo& previous_marker) {
    // compute to_prev transform
    // cout << current_marker.current_camera_pose_position << endl;
    // cout << previous_marker.current_camera_pose_position << endl;
    current_marker.to_prev_position = current_marker.current_camera_pose_position - previous_marker.current_camera_pose_position;
    // cout << current_marker.to_prev_position << endl;
    // cout << norm(current_marker.to_prev_position) << endl;
    current_marker.to_prev_orientation = current_marker.current_camera_pose_orientation * inv(previous_marker.current_camera_pose_orientation);

    // compute current marker transform
    current_marker.world_position = previous_marker.world_position - current_marker.to_prev_position;
    current_marker.world_orientation = previous_marker.world_orientation * inv(current_marker.to_prev_orientation);
}

int main() {
    // Prepare fixed information
    int num_of_markers = 12;
    float marker_length = 0.050f; // meters
    vector<MarkerInfo> markers(num_of_markers);
    vector<vector<Point2f>> corners, rejects;
    Ptr<aruco::DetectorParameters> parameters = aruco::DetectorParameters::create();

    bool first_marker_detected = false;
    int lowest_marker_id = -1;

    // Generate Marker Images
    generateArucoMarkers(num_of_markers);

    // Generate output folder and file
    _mkdir("Results");
    ofstream outputFile;
    outputFile.open("Results/results.csv");
    // if(!outputFile.fail()) outputFile << "MaxNumMarkers: " << num_of_markers << endl;
    // else cout << "Cannot create output file" << endl;

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
    VideoCapture cap("InputVideos/Input_Video_1.mp4");
    // VideoCapture cap(0);
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
        cout << endl << "you are: " << frame_num << endl;

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

            aruco::drawAxis(frame, cameraMatrix, distCoeff, rvecs[i], tvecs[i], 0.1f);
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

            if(!outputFile.fail()) {
                outputFile << "Missing" << endl;
            }

            continue;
        }
        // markers are detected


        // handle first detected marker
        if(!first_marker_detected) {
            first_marker_detected = true;

            // detect the marker with lowest ID
            int lowest_i = 0;
            lowest_marker_id = detected_IDs[lowest_i];
            for(int i = 1; i < num_detected; i++) {
                if(detected_IDs[i] < lowest_marker_id) {
                    lowest_marker_id = detected_IDs[i];
                    lowest_i = i;
                }
            }

            // set it to world's origin
            markers[0].marker_id = lowest_marker_id;

            markers[0].world_position = Vec3d(0, 0, 0);
            markers[0].world_orientation = Quat<double>(1, 0, 0, 0);

            // relative position equals Global position
            markers[0].to_prev_position = Vec3d(0, 0, 0);
            markers[0].to_prev_orientation = Quat<double>(1, 0, 0, 0);

            // camera position is trivial
            getCameraPoseBasedOnMarker(markers[0], rvecs[lowest_i], tvecs[lowest_i]);
            markerPoseInverse(markers[0]);

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
                markerPoseInverse(markers[index]);

                /*
                // NOTE: might be unneceserry here
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
                    // compute transform between two markers and apply world transform
                    computeTransforms(markers[index], markers[last_marker_id]);

                    marker_counter++;
                    markerPoseInverse(markers[index]);
                }
                */
            } else {
                // old marker detected
                getCameraPoseBasedOnMarker(markers[index], rvecs[i], tvecs[i]);
                markerPoseInverse(markers[index]);
            }

            // TODO: check if this is even needed, since the world transform is calculated when previous transform is calculated
            /*
            // compute global position of a new marker
            if(marker_counter_previous < marker_counter) {  // NOTE: this might be problematic or cause unnecesarry computations
                // TODO: calculate geometry to world transform
                // TODO: save the world transform to MarkerInfo
            }
            */

            // check if the marker has a detected previous marker
            if(markers[index].previous_marker_id == -1) {
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
                    // compute transform between two markers and apply world transform
                    computeTransforms(markers[index], markers[last_marker_id]);

                    marker_counter++;
                    // markerPoseInverse(markers[index]);
                }
            }
        }

        // compute which of the visible markers is the closest to the camera
        double minimal_distance = INIT_MIN_SIZE_VALUE;
        int closest_index = -1;
        for(int k = 0; k < num_of_markers; k++) {
            
            // check if marker is visible
            if(markers[k].visible) {
                double a = markers[k].current_camera_pose_position.val[0];
                double b = markers[k].current_camera_pose_position.val[1];
                double c = markers[k].current_camera_pose_position.val[2];
                double size = sqrt(a*a + b * b + c * c);
                
                if(size < minimal_distance) {
                    minimal_distance = size;
                    closest_index = k;
                }
            }
        }

        // check if closest marker has world position
        if(markers[closest_index].previous_marker_id != -1) {
            // compute global camera pose
            Vec3d global_cam_position = markers[closest_index].world_position + markers[closest_index].current_camera_pose_position;
            Quat<double> global_cam_orientation = markers[closest_index].world_orientation + markers[closest_index].current_camera_pose_orientation;

            // output
            cout << "Frame: " << frame_num << endl;
            cout << "Camera pose based on marker: " << markers[closest_index].marker_id << endl;
            cout << "t: " << global_cam_position << endl;
            cout << "r: " << global_cam_orientation << endl;
            cout << "d: " << minimal_distance << endl;

            if(!outputFile.fail()) {
                /*
                outputFile << "Frame: " << frame_num << endl;
                outputFile << "NumMarkers: " << num_detected << endl;
                outputFile << "ClosestMarker: " << markers[closest_index].marker_id << endl;
                outputFile << "T: " << global_cam_position << endl;
                outputFile << "R: " << global_cam_orientation << endl;
                */
                outputFile << "Frame," << frame_num << ",";
                outputFile << num_detected << ",";
                outputFile << markers[closest_index].marker_id << ",";
                outputFile << global_cam_position.val[0] << ",";
                outputFile << global_cam_position.val[1] << ",";
                outputFile << global_cam_position.val[2] << ",";
                outputFile << global_cam_orientation.x << ",";
                outputFile << global_cam_orientation.y << ",";
                outputFile << global_cam_orientation.z << ",";
                outputFile << global_cam_orientation.w;

                for(int i = 0; i < num_of_markers; i++) {
                    if(markers[i].visible) {
                        outputFile << "," << markers[i].marker_id;
                    }
                }
                outputFile << endl;
            }
        } else {
            cout << "Cannot get accurate cam position" << endl;
            
            if(!outputFile.fail()) {
                outputFile << "Missing" << endl;
            }
        }
    }

    if(!outputFile.fail()) {
        outputFile << "MaxNumMarkers," << num_of_markers << endl;

        for(int i = 0; i < num_of_markers; i++) {
            /*
            outputFile << "ID: " << markers[i].marker_id << endl;
            outputFile << "T: " << markers[i].world_position << endl;
            outputFile << "R: " << markers[i].world_orientation << endl;
            */
            outputFile << markers[i].marker_id;
            outputFile << "," << markers[i].world_position.val[0];
            outputFile << "," << markers[i].world_position.val[1];
            outputFile << "," << markers[i].world_position.val[2];
            outputFile << "," << markers[i].world_orientation.x;
            outputFile << "," << markers[i].world_orientation.y;
            outputFile << "," << markers[i].world_orientation.z;
            outputFile << "," << markers[i].world_orientation.w;
            outputFile << endl;
        }
    }

    outputFile.close();

    return 0;
}