// VideoSLAM.cpp : Defines the entry point for the console application.

#include "stdafx.h"

#define SAVEFRAME_PATH "DeconVid/"  // WARNING: be careful of what you write in here :3

// INPUT
#define VIDEO_PATH "InputVideos/SF_Video_5.mp4"
#define SENSOR_DATA_FILE_PATH "InputVideos/SF_Data_5.txt"

// OUTPUT
#define RESULTS_FILE_PATH "Results/Ground_Truth_Data.csv"
#define REWRITTEN_IMU_DATA_FILE_PATH "Results/IMU_Data.txt"
#define OUTPUT_VIDEO_PATH "Results/Used_Video.mp4"

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

    // set to MarkerInfo
    marker.my_position = t_vec;
    marker.my_orientation_matrix = marker_rotation.t();
    // cout << marker.my_orientation_matrix << endl;
    marker.my_orientation = Quat<double>::createFromRotMat(marker.my_orientation_matrix);
    // cout << marker.my_orientation << endl;

    marker.current_camera_pose_orientation_matrix = marker.my_orientation_matrix.inv();
    marker.current_camera_pose_orientation = Quat<double>::createFromRotMat(marker.current_camera_pose_orientation_matrix);
    // cout << marker.current_camera_pose_orientation << endl;
    marker.current_camera_pose_position = ((cv::Matx33d) marker.current_camera_pose_orientation_matrix).t() * -t_vec;
    // cout << marker.current_camera_pose_position << endl;
}

/// <sumary>
/// Performs a transform inversion on the marker's camera_pose_position and orientation.
/// </sumary>
/// <param name="marker">MarkerInfo of the marker, on which the camera_pose will be inverted</param>
void markerPoseInverseDEPRICATED(MarkerInfo& marker) {
    // rotation inverse
    // TODO: fix
    // marker.current_camera_pose_orientation = inv(marker.current_camera_pose_orientation);
    marker.current_camera_pose_orientation_matrix = marker.current_camera_pose_orientation_matrix.inv();
    marker.current_camera_pose_orientation = Quat<double>::createFromRotMat(marker.current_camera_pose_orientation_matrix);
    cout << marker.current_camera_pose_orientation.toEulerAngles(QuatEnum::INT_ZYX) << endl;

    // translation inverse
    auto marker_translation = marker.current_camera_pose_orientation.toRotMat3x3() * -marker.current_camera_pose_position;
    // TODO: check if correct, although this should work in world coordinates
    // auto marker_translation = -marker.current_camera_pose_position;

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
    cout << endl;
    Vec3d tran_diff = current_marker.my_position - previous_marker.my_position;
    // cout << "tran_diff: " << tran_diff << endl;
    Vec3d trenutni_v_bazicnem_ks = ((Matx33d) (Mat) previous_marker.my_orientation_matrix.inv().t() * tran_diff); // konec centriranja v koordinatni sistem bazicnega markerja
    // cout << "trenutni_v_bazicnem_ks: " << trenutni_v_bazicnem_ks << endl;
    
    // cout << "previous_marker.my_orientation_matrix: " << endl << previous_marker.my_orientation_matrix << endl;
    // cout << "previous_marker.current_camera_pose_orientation_matrix: " << endl << previous_marker.current_camera_pose_orientation_matrix << endl;
    // cout << "current_marker.my_orientation_matrix: " << endl << current_marker.my_orientation_matrix << endl;
    // Mat current_rot_matrix_v_bazicnem_ks = previous_marker.my_orientation_matrix.inv() * current_marker.my_orientation_matrix;
    // Mat current_rot_matrix_v_bazicnem_ks = (previous_marker.my_orientation_matrix.inv().t() * current_marker.my_orientation_matrix).t();
    Mat current_rot_matrix_v_bazicnem_ks = current_marker.my_orientation_matrix * previous_marker.my_orientation_matrix.inv();
    // cout << "current_rot_matrix_v_bazicnem_ks: " << endl << current_rot_matrix_v_bazicnem_ks << endl << endl;

    Vec3d centriran_superb_v_bazicnem_ks = ((Matx33d) (Mat) previous_marker.world_orientation_matrix.inv().t()) * -previous_marker.world_position;// centriran superbazicni v koordinatni sistem bazicnega
    // cout << "centriran_superb_v_bazicnem_ks: " << endl << centriran_superb_v_bazicnem_ks << endl << endl;

    // cout << "previous_marker.world_orientation_matrix: " << endl << previous_marker.world_orientation_matrix << endl;
    // trenutni_v_bazicnem_ks = trenutni_v_bazicnem_ks - centriran_superb_v_bazicnem_ks;
    // current_marker.world_position = ((Matx33d) (Mat) previous_marker.world_orientation_matrix) * trenutni_v_bazicnem_ks;
    current_marker.world_position = ((Matx33d) (Mat) previous_marker.world_orientation_matrix.t()) * (trenutni_v_bazicnem_ks - centriran_superb_v_bazicnem_ks);
    // current_marker.world_orientation_matrix = previous_marker.world_orientation_matrix * current_rot_matrix_v_bazicnem_ks;
    current_marker.world_orientation_matrix = (previous_marker.world_orientation_matrix.t() * current_rot_matrix_v_bazicnem_ks.t()).t();
    // current_marker.world_orientation_matrix = (previous_marker.world_orientation_matrix * current_rot_matrix_v_bazicnem_ks).inv();
    current_marker.world_orientation = Quat<double>::createFromRotMat(current_marker.world_orientation_matrix);

    cout << "New marker: " << current_marker.world_position << endl << current_marker.world_orientation_matrix << endl << endl;

    // previous_marker.world_orientation_matrix - kako superbaizcen vidi bazicnega
    // previous_marker.world_position - kako superbaizcen vidi bazicnega

    /*
    // world transform
    current_marker.world_position = ((Matx33d) rot_diff_matrix) * tran_diff + previous_marker.world_position;
    current_marker.world_orientation_matrix = rot_diff_matrix * current_marker.current_camera_pose_orientation_matrix;
    current_marker.world_orientation = Quat<double>::createFromRotMat(current_marker.world_orientation_matrix);


    // transform to previous marker
    current_marker.to_prev_position = previous_marker.world_position - current_marker.world_position;
    current_marker.to_prev_orientation_matrix = previous_marker.world_orientation_matrix * current_marker.world_orientation_matrix.inv();
    current_marker.to_prev_orientation = Quat<double>::createFromRotMat(current_marker.to_prev_orientation_matrix);
    */
 }

int rewriteIMUfile(string imu_data_file, long* start_sec, long* start_nano) {
    fstream imuFile;
    imuFile.open(imu_data_file);
    if(imuFile.fail()) {
        cerr << "Could not read IMU data file: " + imu_data_file << endl;
        return -4;
    }

    string line;
    if(!getline(imuFile, line)) {
        cerr << "Wrong IMU file format" << endl;
        return -5;
    }
    string nanosec = line.substr(line.size() - 9, 9);
    string sec = line.substr(12, line.size() - 9 - 12);

    *start_nano = stol(nanosec);
    *start_sec = stol(sec);

    // 1. create data.csv
    ofstream outStream(REWRITTEN_IMU_DATA_FILE_PATH);
    if(outStream) {
        // 2. write the comment line
        outStream << "#timestamp [ns],w_RS_S_x [rad s^-1],w_RS_S_y [rad s^-1],w_RS_S_z [rad s^-1],a_RS_S_x [m s^-2],a_RS_S_y [m s^-2],a_RS_S_z [m s^-2]" << endl;
        
        // 3. copy data line by line into memory:
        // use three queues, one for strings, two for floats
        queue<string> timestamps;
        queue<string> gyr_data;
        queue<string> acc_data;
        for(int i = 0; getline(imuFile, line); i++) {
            // check first string
            stringstream stream(line);
            string s;
            getline(stream, s, ' ');
            
            if(s.compare("VIDEO_STOP") == 0) break;  // end of data

            string sys_time_data, event_time_data;
            getline(stream, sys_time_data, ' ');
            getline(stream, event_time_data, ' ');

            
            if(s.compare("A") == 0) {
                for(int j = 0; j < 3; j++) {
                    getline(stream, s, ' ');
                    acc_data.push(s);
                }
            } else if(s.compare("G") == 0) {
                for(int j = 0; j < 3; j++) {
                    getline(stream, s, ' ');
                    gyr_data.push(s);
                }
            } else if(s.compare("M") == 0) {
                // Magnetometer: TODO
                continue;
            } else {
                // end of data
                break;
            }

            // 4. write data line by line into file
            if(acc_data.size() > 0 && gyr_data.size() > 0) {
                // data is ready, dont remember time
                outStream << timestamps.front();
                timestamps.pop();

                for(int j = 0; j < 3; j++) {
                    outStream << "," << gyr_data.front();
                    gyr_data.pop();
                }
                for(int j = 0; j < 3; j++) {
                    outStream << "," << acc_data.front();
                    acc_data.pop();
                }

                outStream << endl;

            } else {
                // new data, remember time
                timestamps.push(sys_time_data);
            }
        }
        
        // 5. close file
        outStream.close();
    } else {
        // TODO: handle file not opening
    }
    imuFile.close();
    return 0;
}


int rewriteIMUfileToLibRSF(string imu_data_file, long* start_sec, long* start_nano) {
    fstream imuFile;
    imuFile.open(imu_data_file);
    if(imuFile.fail()) {
        cerr << "Could not read IMU data file: " + imu_data_file << endl;
        return -4;
    }

    string line;
    if(!getline(imuFile, line)) {
        cerr << "Wrong IMU file format" << endl;
        return -5;
    }
    string nanosec = line.substr(line.size() - 9, 9);
    string sec = line.substr(12, line.size() - 9 - 12);

    *start_nano = stol(nanosec);
    *start_sec = stol(sec);

    // 1. create data.csv
    ofstream outStream(REWRITTEN_IMU_DATA_FILE_PATH);
    if(outStream) {

        // 3. copy data line by line into memory:
        // use three queues, one for strings, two for floats
        queue<string> timestamps;
        queue<string> gyr_data;
        queue<string> acc_data;
        Vec3d current_velocity(0,0,0);
        vector<double> tss(0);
        int output_line_num = -1;
        for(int i = 0; getline(imuFile, line); i++) {
            // check first string
            stringstream stream(line);
            string s;
            getline(stream, s, ' ');

            if(s.compare("VIDEO_STOP") == 0) break;  // end of data

            string sys_time_data, event_time_data;
            getline(stream, sys_time_data, ' ');
            getline(stream, event_time_data, ' ');


            if(s.compare("A") == 0) {
                for(int j = 0; j < 3; j++) {
                    getline(stream, s, ' ');
                    acc_data.push(s);
                }
            } else if(s.compare("G") == 0) {
                for(int j = 0; j < 3; j++) {
                    getline(stream, s, ' ');
                    gyr_data.push(s);
                }
            } else if(s.compare("M") == 0) {
                // Magnetometer: TODO
                continue;
            } else {
                break;
            }

            // 4. write data line by line into file
            if(acc_data.size() > 0 && gyr_data.size() > 0) {
                // data is ready, dont remember time
                // outStream << timestamps.front();
                string curr_timestamp = timestamps.front();
                timestamps.pop();
                nanosec = curr_timestamp.substr(curr_timestamp.size() - 9, 9);
                sec = curr_timestamp.substr(0, curr_timestamp.size() - 9);

                double ts = atof((sec + "." + nanosec).c_str());
                // cout << ts << endl;
                tss.push_back(ts);
                output_line_num++;


                // 1 - "odom3" [string]                        odom3
                outStream << "odom3 ";
                // 2 - time stamp [s]                          0.19999980926514
                // outStream << sec << "." << nanosec << " ";
                outStream << tss[output_line_num] << " ";


                // get acceleration [m/s^2]
                Vec3d acc(3);
                for(int j = 0; j < 3; j++) {
                    acc[j] = atof(acc_data.front().c_str());
                    acc_data.pop();
                }
                if(output_line_num > 0) {
                    double dt = tss[output_line_num] - tss[output_line_num-1];
                    current_velocity += acc * dt / 2;
                }

                // 3 - velocity X-Axis [m/s]                   0
                outStream << current_velocity[0] << " ";
                // 4 - velocity Y-Axis [m/s]                   0
                outStream << current_velocity[1] << " ";
                // 5 - velocity Z-Axis [m/s]                   0
                outStream << current_velocity[2] << " ";


                Vec3d gyr(3);
                for(int j = 0; j < 3; j++) {
                    gyr[j] = atof(gyr_data.front().c_str());
                    gyr_data.pop();
                }
                // 6 - turn rate X-Axis [rad/s]                0
                outStream << gyr[0] << " ";
                // 7 - turn rate Y-Axis [rad/s]                0
                outStream << gyr[1] << " ";
                // 8 - turn rate Z-Axis [rad/s]                -0.00034906585039887
                outStream << gyr[2] << " ";


                // 9  - covariance velocity X-Axis [m/s]^2     0.0025
                outStream << 0.0025 << " ";
                // 10 - covariance velocity Y-Axis [m/s]^2     0.0009
                outStream << 0.0009 << " ";
                // 11 - covariance velocity Z-Axis [m/s]^2     0.0009
                outStream << 0.0009 << " ";


                // 12 - covariance turn rate X-Axis [rad/s]^2  4e-06
                outStream << 4e-06 << " ";
                // 13 - covariance turn rate Y-Axis [rad/s]^2  4e-06
                outStream << 4e-06 << " ";
                // 14 - covariance turn rate Z-Axis [rad/s]^2  4e-06
                outStream << 4e-06 << " ";


                outStream << endl;

            } else {
                // new data, remember time
                timestamps.push(sys_time_data);
            }
        }

        // add "GPS" data
        // TODO: include actual data
        for(int i = 0; i < tss.size(); i++) {

            // ## pseudoranges ##
            // 1 - "pseudorange3"[string]                         pseudorange3
            outStream << "pseudorange3" << " ";
            // 2 - time stamp[s]                                  0
            outStream << to_string(tss[i]) << " ";
            // 3 - pseudorange mean[m]                            0
            outStream << 0 << " ";
            // 4 - pseudorange covariance[m] ^ 2                  100
            outStream << 100 << " ";
            // 5 - satellite position ECEF - X[m]                 0
            outStream << 0 << " ";
            // 6 - satellite position ECEF - Y[m]                 0
            outStream << 0 << " ";
            // 7 - satellite position ECEF - Z[m]                 0
            outStream << 0 << " ";
            // 8 - satellite ID                                   0
            outStream << 0 << " ";
            // 9 - satellite elevation angle[deg]                 0
            outStream << 0 << " ";
            // 10 - carrier - to - noise density ratio[dBHz]      0
            outStream << 0 << endl;
        }

        // 5. close file
        outStream.close();
    } else {
        // TODO: handle file not opening
    }
    imuFile.close();
    return 0;
}


int main() {
    /*
    vector<MarkerInfo> m_marker(2);

    m_marker[0].world_position = Vec3d(0, 0, 0);
    // markers[0].world_orientation = Quat<double>(1, 0, 0, 0);
    m_marker[0].world_orientation_matrix = Mat::eye(3, 3, CV_64F);
    m_marker[0].world_orientation = Quat<double>::createFromRotMat(m_marker[0].world_orientation_matrix);

    // relative position equals Global position
    m_marker[0].to_prev_position = Vec3d(0, 0, 0);

    // markers[0].to_prev_orientation = Quat<double>(1, 0, 0, 0);
    m_marker[0].to_prev_orientation_matrix = Mat::eye(3, 3, CV_64F);
    m_marker[0].to_prev_orientation = Quat<double>::createFromRotMat(m_marker[0].to_prev_orientation_matrix);
    

    Vec3d in_t_vec = Vec3d(0, 4, 0);
    cout << in_t_vec << endl;
    Mat in_rot_matrix = Mat::eye(3, 3, CV_64F);
    in_rot_matrix.at<double>(0, 0) = 0;
    in_rot_matrix.at<double>(0, 1) = 1;
    in_rot_matrix.at<double>(0, 2) = 0;
    in_rot_matrix.at<double>(1, 0) = -1;
    in_rot_matrix.at<double>(1, 1) = 0;
    in_rot_matrix.at<double>(1, 2) = 0;
    in_rot_matrix.at<double>(2, 0) = 0.0;
    in_rot_matrix.at<double>(2, 1) = 0.0;
    in_rot_matrix.at<double>(2, 2) = 1.0;
    cout << in_rot_matrix << endl << endl;

    Mat rhs2lhs = Mat::eye(3, 3, CV_64F);
    rhs2lhs.at<double>(0, 0) = 1;
    rhs2lhs.at<double>(0, 1) = 0;
    rhs2lhs.at<double>(0, 2) = 0;
    rhs2lhs.at<double>(1, 0) = 0;
    rhs2lhs.at<double>(1, 1) = -1;
    rhs2lhs.at<double>(1, 2) = 0;
    rhs2lhs.at<double>(2, 0) = 0.0;
    rhs2lhs.at<double>(2, 1) = 0.0;
    rhs2lhs.at<double>(2, 2) = 1.0;
    cout << rhs2lhs << endl << endl;

    Quat<double> q1 = Quat<double>::createFromRotMat(in_rot_matrix);
    Quat<double> q2 = Quat<double>::createFromRotMat(rhs2lhs*in_rot_matrix);
    Quat<double> q3(0, 1, 0, 0);
    cout << q1 << endl;
    // cout << q2 << endl;
    // cout << Quat<double>::createFromRotMat(q3.toRotMat3x3()) << endl;
    // cout << q3 * q3.inv() << endl;
    // cout << Quat<double>::createFromRotMat(q3.toRotMat3x3()) * q3.inv() << endl;

    // set to MarkerInfo
    m_marker[0].current_camera_pose_orientation_matrix = in_rot_matrix.inv();
    m_marker[0].current_camera_pose_orientation = Quat<double>::createFromRotMat(m_marker[0].current_camera_pose_orientation_matrix);
    m_marker[0].current_camera_pose_position = ((cv::Matx33d) m_marker[0].current_camera_pose_orientation_matrix) * -in_t_vec;

    // cout << m_marker[0].current_camera_pose_position << endl;
    // cout << m_marker[0].current_camera_pose_orientation_matrix << endl;

    return 0;
    */


    // Prepare fixed information
    int num_of_markers = 12;
    bool visualize = true;
    float marker_length = 0.050f; // meters
    vector<MarkerInfo> markers(num_of_markers);
    vector<vector<Point2f>> corners, rejects;
    Ptr<aruco::DetectorParameters> parameters = aruco::DetectorParameters::create();

    bool save_frames = true;                // should the video break down the video into frames
    string save_frames_path = "DeconVid/";  // path to the directory to whic the frames will be saved

    bool first_marker_detected = false;
    int lowest_marker_id = -1;

    // Generate Marker Images
    generateArucoMarkers(num_of_markers);

    // Generate output folder and file
    _mkdir("Results");
    ofstream outputFile;
    outputFile.open(RESULTS_FILE_PATH);
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
    // VideoCapture cap("InputVideos/Input_Video_1.mp4");
    // VideoCapture cap("InputVideos/real_set_1/D1_WM_A_V2.mp4");
    // VideoCapture cap("InputVideos/real_set_2/SF_Video_9.mp4");
    VideoCapture cap(VIDEO_PATH);
    // VideoCapture cap("InputVideos/set_5_rot_3/SF_Video_3.mp4");
    // VideoCapture cap(0);
    if(!cap.isOpened()) {
        cout << "Cannot open the input video file" << endl;
        cin.get();
        return -1;
    }

    if(save_frames) {
        // check if there is a save directory
        int is_dir_made = _mkdir(SAVEFRAME_PATH);
        if(is_dir_made != 0) {
            // the directory exists or cannot be created
            is_dir_made = experimental::filesystem::remove_all(SAVEFRAME_PATH);
            if(is_dir_made >= 0) {
                is_dir_made = _mkdir(SAVEFRAME_PATH);
            }

            if(is_dir_made != 0){
                cerr << "Cannot create saveframe directory" << endl;
                return -2;
            }
        }
        // directory now exists
    }

    
    // rewrite used video to indicate which video was used
    remove(OUTPUT_VIDEO_PATH);
    if(experimental::filesystem::copy_file(VIDEO_PATH, OUTPUT_VIDEO_PATH)) {
        cout << "Input video saved" << endl;
    }


    double fps = cap.get(CAP_PROP_FPS);
    std::cout << "Frames per seconds : " << fps << endl;


    long start_nano, start_sec;
    // int rewrite_res = rewriteIMUfileToLibRSF(SENSOR_DATA_FILE_PATH, &start_sec, &start_nano);
    int rewrite_res = rewriteIMUfile(SENSOR_DATA_FILE_PATH, &start_sec, &start_nano);
    if(rewrite_res != 0) return rewrite_res;

    // Visualization code
    String window_name = "Video Preview";
    if(visualize) {
        namedWindow(window_name, WINDOW_NORMAL);
    }

    // Work on the video frame-by-frame
    Mat frame;
    int marker_counter = 0;
    for(int frame_num = 0; cap.read(frame); frame_num++) {
        if(save_frames) {
            long sec, nano;
            double tmp = round(frame_num / fps * 1000000000) + start_nano;
            nano = floor(tmp-floor(tmp/1000000000)*1000000000);
            // cout << tmp << endl;
            // cout << nano << endl;
            // cout << floor(tmp/1000000000) << endl;
            sec = start_sec + floor(tmp / 1000000000);
            // cout << sec << endl;

            // save this frame as an image
            string img_name = format("%ld%09ld.png", sec, nano);
            // cout << img_name << endl;
            bool created = imwrite(save_frames_path + img_name, frame);
            // cin.get();
            // return 1;
        }

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
        if(visualize) {
            aruco::drawDetectedMarkers(frame, corners, detected_IDs);
            // cout << marker_IDs.size() << endl;
            for(int i = 0; i < detected_IDs.size(); i++) {
                cout << detected_IDs[i] << endl;

                aruco::drawAxis(frame, cameraMatrix, distCoeff, rvecs[i], tvecs[i], 0.1f);
            }
            imshow(window_name, frame);
            cout << endl;
            // end visualization code

            if(waitKey(1) == 27) {
                cout << "ESC pressed" << endl;
                break;
            }
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
            // markers[0].world_orientation = Quat<double>(1, 0, 0, 0);
            markers[0].world_orientation_matrix = Mat::eye(3, 3, CV_64F);
            markers[0].world_orientation = Quat<double>::createFromRotMat(markers[0].world_orientation_matrix);

            // relative position equals Global position
            markers[0].to_prev_position = Vec3d(0, 0, 0);
            // markers[0].to_prev_orientation = Quat<double>(1, 0, 0, 0);
            markers[0].to_prev_orientation_matrix = Mat::eye(3, 3, CV_64F);
            markers[0].to_prev_orientation = Quat<double>::createFromRotMat(markers[0].to_prev_orientation_matrix);

            // camera position is trivial
            getCameraPoseBasedOnMarker(markers[0], rvecs[lowest_i], tvecs[lowest_i]);
            // markerPoseInverse(markers[0]);

            // increase count
            marker_counter++;
            
            // visibility
            markers[0].visible = true;

            // previous marker
            markers[0].previous_marker_index = WORLD;

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
                // markerPoseInverse(markers[index]);

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
                // markerPoseInverse(markers[index]);
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
            if(markers[index].previous_marker_index == -1) {
                // test if position can be calculated based on old known marker
                bool any_known_marker_visible = false;
                int last_marker_id;
                for(int k = 0; k < index && !any_known_marker_visible; k++) {
                    if(markers[k].visible == true && markers[k].previous_marker_index != -1) {
                        any_known_marker_visible = true;
                        markers[index].previous_marker_index = k;
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
        if(markers[closest_index].previous_marker_index != -1) {
            // compute global camera pose
            // Vec3d global_cam_position = markers[closest_index].world_position + markers[closest_index].current_camera_pose_position;
            // Quat<double> global_cam_orientation = markers[closest_index].world_orientation * markers[closest_index].current_camera_pose_orientation;
            // Vec3d global_cam_position = (Matx33d) (Mat) markers[closest_index].world_orientation_matrix * (markers[closest_index].current_camera_pose_position - ((Matx33d) (Mat) markers[closest_index].world_orientation_matrix.inv()) * (-markers[closest_index].world_position));
            // cout << endl;
            Vec3d global_cam_position = markers[closest_index].world_position + (Matx33d) (Mat) markers[closest_index].world_orientation_matrix.inv() * markers[closest_index].current_camera_pose_position;
            // cout << markers[closest_index].world_orientation_matrix << endl;
            // cout << markers[closest_index].current_camera_pose_orientation_matrix << endl;
            // Mat global_cam_orientation_matrix = markers[closest_index].world_orientation_matrix.t() * markers[closest_index].current_camera_pose_orientation_matrix;
            Mat global_cam_orientation_matrix = markers[closest_index].current_camera_pose_orientation_matrix * markers[closest_index].world_orientation_matrix;
            Quat<double> global_cam_orientation = Quat<double>::createFromRotMat(global_cam_orientation_matrix);

            cout << global_cam_position << endl;
            cout << global_cam_orientation.toRotMat3x3() << endl;

            // output
            // cout << "Frame: " << frame_num << endl;
            // cout << "Camera pose based on marker: " << markers[closest_index].marker_id << endl;
            // cout << "t: " << global_cam_position << endl;
            // cout << "r: " << global_cam_orientation << endl;
            // cout << "d: " << minimal_distance << endl;

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