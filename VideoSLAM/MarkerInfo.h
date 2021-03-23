#ifndef MARKER_INFO_H
#define MARKER_INFO_H


class MarkerInfo {
public:
    // attributes
    bool visible;
    int previous_marker_id;
    int marker_id;

    // relative to world transformation
    float world_position_x;
    float world_position_y;
    float world_position_z;
    float world_orientation_x;
    float world_orientation_y;
    float world_orientation_z;
    float world_orientation_w;

    // relative to previous marker transformation
    float to_prev_position_x;
    float to_prev_position_y;
    float to_prev_position_z;
    float to_prev_orientation_x;
    float to_prev_orientation_y;
    float to_prev_orientation_z;
    float to_prev_orientation_w;

    // current camera pose
    // TODO: chose which is actually needed
    float current_camera_pose_position_x;
    float current_camera_pose_position_y;
    float current_camera_pose_position_z;

    cv::Mat current_camera_pose_position;
    // Mat current_camera_pose_orientation;
    cv::Quat<double> current_camera_pose_orientation;



    // constructors & deconstructors
    MarkerInfo();
    virtual ~MarkerInfo();

    // methods
};

#endif