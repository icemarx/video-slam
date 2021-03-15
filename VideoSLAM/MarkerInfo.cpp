#include "stdafx.h"
#include "MarkerInfo.h"

MarkerInfo::MarkerInfo() {
    this->visible = false;
    this->previous_marker_id = -1;
    this->marker_id = -1;
}

MarkerInfo::~MarkerInfo() = default;