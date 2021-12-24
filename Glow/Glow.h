#ifndef GLOW_H
#define GLOW_H

#include <CubeApplication.h>
#include <Mpu6050.h>

class Glow : public CubeApplication{
public:
    Glow();
    bool loop();
private:
    double hue_;
    Mpu6050 Imu;
};

#endif //GLOW_H
