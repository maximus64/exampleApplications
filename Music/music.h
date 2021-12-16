#ifndef __MUSIC_H__
#define __MUSIC_H__

#include <CubeApplication.h>

class Music : public CubeApplication{
public:
    Music();
    bool loop();
private:
    void playerLoop();
    std::unique_ptr<std::thread> thread_;
    float amplitute_;
    double db_mag_[128];
    double db_smooth_[128];
    double maxVal_;
    double minVal_;
};


#endif
