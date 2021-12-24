#include "Glow.h"
#include <algorithm>
#include <cmath>

Glow::Glow() : CubeApplication(30), hue_(0){
}

bool Glow::loop() {
    Color col;
    const auto imu = Imu.getAcceleration();
    const Eigen::Vector3f origin = {0,0,-1};
    constexpr double smooth_factor = 0.8;

    double angle = std::atan2(imu.cross(origin).norm(), imu.dot(origin)) * 360 / M_PI;
    hue_ = hue_ * smooth_factor + (1.0 - smooth_factor) * angle;

    col.fromHSV(hue_, 1, 1);

    fillAll(col);

    render();
    return true;
}
