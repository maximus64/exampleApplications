#ifndef __HBD_H__
#define __HBD_H__

#include <CubeApplication.h>
#include <vector>
#include <Joystick.h>
#include <BattSensor.h>
#include <thread>

class HBD : public CubeApplication{
public:
    HBD();
    bool loop();
private:
    enum MenuState {
        MS_SELECT, MS_PLAYING, MS_END
    };
    void decodeLoop();
    void playingDraw(unsigned loopcount);

    JoystickManager joystickmngr;
    BattSensor adcBattery;
    std::unique_ptr<std::thread> thread_;
    MenuState state_;
    std::string hostname_;
    float volume_, accel_volume_;
    Image sprites_;
    char lip_state_;
};


#endif
