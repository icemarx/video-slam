#ifndef MARKER_INFO_H
#define MARKER_INFO_H


class MarkerInfo {
public:
    // attributes
    bool visible;
    int previous_marker_id;
    int marker_id;

    // relative to world transformation
    cv::Vec3d world_position;
    cv::Quat<double> world_orientation;

    // relative to previous marker transformation
    cv::Vec3d to_prev_position;
    cv::Quat<double> to_prev_orientation;

    // current camera pose
    cv::Vec3d current_camera_pose_position;
    cv::Quat<double> current_camera_pose_orientation;



    // constructors & deconstructors
    MarkerInfo();
    virtual ~MarkerInfo();

    // methods
};

#endif