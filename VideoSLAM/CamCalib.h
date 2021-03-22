#ifndef CAMCALIB_H
#define CAMCALIB_H

#include "stdafx.h"

class CamCalib {
    public:
        int myCalibrateCamera(std::string fileName, bool preview);
        CamCalib();
        virtual ~CamCalib();
};

#endif // CAMCALIB_H