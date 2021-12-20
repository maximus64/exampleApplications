#ifndef __MUSIC_H__
#define __MUSIC_H__

#include <CubeApplication.h>
#include <vector>
#include <Joystick.h>
#include <BattSensor.h>
#include <thread>

class Music : public CubeApplication{
public:
    Music();
    bool loop();
private:
    enum MenuState {
        MS_SELECT, MS_PLAYING, MS_END
    };
    struct SongInfo {
        SongInfo(std::string name, std::string path, Color col = Color::white()) 
            : name(name), path(path), color(col) {
        }
        std::string name;
        std::string path;
        Color color;
    };
    struct SongInfo;
    void decodeLoop();
    void playingDraw();
    void selectDraw(unsigned loopcount);

    JoystickManager joystickmngr;
    BattSensor adcBattery;
    std::unique_ptr<std::thread> thread_;
    float amplitute_;
    double db_mag_[128];
    double db_smooth_[128];
    double db_weight_[128];
    double maxVal_;
    double minVal_;
    float line_hue_;
    std::vector<SongInfo> songs_list_;
    MenuState state_;
    std::string hostname_;
    int song_idx_;
};


#endif
