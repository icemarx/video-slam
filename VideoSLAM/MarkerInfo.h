#ifndef MARKER_INFO_H
#define MARKER_INFO_H


class MarkerInfo {
public:
    // attributes
    bool visible;
    int previous_marker_index;
    int marker_id;

    // relative to world transformation
    cv::Vec3d world_position;
    cv::Quat<double> world_orientation;
    cv::Mat world_orientation_matrix;

    // relative to previous marker transformation
    cv::Vec3d to_prev_position;
    cv::Quat<double> to_prev_orientation;
    cv::Mat to_prev_orientation_matrix;

    // current marker pose based on camera
    cv::Vec3d my_position;
    cv::Quat<double> my_orientation;
    cv::Mat my_orientation_matrix;

    // current camera pose
    cv::Vec3d current_camera_pose_position;
    cv::Quat<double> current_camera_pose_orientation;
    cv::Mat current_camera_pose_orientation_matrix;



    // constructors & deconstructors
    MarkerInfo();
    virtual ~MarkerInfo();

    // methods
};

#endif