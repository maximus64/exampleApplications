#include "HBD.h"
//general
#include <stdio.h>
#include <algorithm>
#include <iterator>
#include <cmath>
#include <random>
#include <chrono>
#include <alsa/asoundlib.h>
#include <alsa/pcm.h>
#include "AudioDecoder.h"
#include <filesystem>

#define JS_DEADZONE 0.5
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))


namespace {
static const Bitmap1bpp battery_indicator = {
    Vector2i(0,0),
    Vector2i(0,1),
    Vector2i(0,2),
    Vector2i(0,3),
    Vector2i(0,4),
    Vector2i(0,5),

    Vector2i(1,0),
    Vector2i(1,5),
    Vector2i(2,0),
    Vector2i(2,5),
    Vector2i(3,0),
    Vector2i(3,5),
    Vector2i(4,0),
    Vector2i(4,5),
    Vector2i(5,0),
    Vector2i(5,5),
    Vector2i(6,0),
    Vector2i(6,5),
    Vector2i(7,0),
    Vector2i(7,5),
    Vector2i(8,0),
    Vector2i(8,5),
    Vector2i(9,0),
    Vector2i(9,5),

    Vector2i(10,0),
    Vector2i(10,1),
    Vector2i(10,2),
    Vector2i(10,3),
    Vector2i(10,4),
    Vector2i(10,5),

    Vector2i(11,1),
    Vector2i(11,2),
    Vector2i(11,3),
    Vector2i(11,4),
};


struct LipSync {
    double pts;
    char type;
};

constexpr LipSync lips[] = {
#include "lipsync.inc"
};

}

HBD::HBD() : 
    CubeApplication(30),
    joystickmngr(8),
    lip_state_('X') {

    char hostname[20];
    gethostname(hostname, sizeof(hostname));
    hostname_ = std::string(hostname);

    state_ = MS_PLAYING;
    thread_ = std::make_unique<std::thread> ( [this](){ decodeLoop(); } );

    sprites_.loadImage(std::string("/root/panda_sprite.png"));
}

void HBD::decodeLoop() {
    int err;
    std::vector<int16_t> buf;

    AudioDecoder ad("/root/hbd.mp3");

    snd_pcm_t *handle;
    if ((err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        printf("Playback open error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }
    if ((err = snd_pcm_set_params(handle,
                            SND_PCM_FORMAT_S16_LE,
                            SND_PCM_ACCESS_RW_INTERLEAVED,
                            1,
                            48000,
                            1,
                            20000)) < 0) {
        printf("Playback open error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    snd_pcm_sframes_t frames;
    frames = snd_pcm_avail(handle);
    printf("PCM frames available: %d\n", frames);

#define NUM_SAMPLE 384
    buf.reserve(NUM_SAMPLE);

    while (getAppState() == AppState::running) {
        auto pts = ad.getTimeStamp();
        int i;
        for (i = 0; i < ARRAY_SIZE(lips); i++) {
            if (lips[i].pts > ad.getTimeStamp())
                break;
        }
        i--;
        if (i < 0) i = 0;
        lip_state_ = lips[i].type;

        auto count = ad.getSample(buf, NUM_SAMPLE);
        if (count <= 0) {
            state_ = MS_END;
            break;
        }
        err = snd_pcm_writei(handle, buf.data(), count);
        if (err < 0)
            err = snd_pcm_recover(handle, err, 0);
        if (err < 0) {
            printf("snd_pcm_writei error: %s\n", snd_strerror(err));
            exit(EXIT_FAILURE);
        }

        buf.clear();
    }

    snd_pcm_drop(handle);
    snd_pcm_close(handle);
}

void HBD::playingDraw(unsigned loopcount) {
    Color colSelectedText;
    clear();

    colSelectedText.fromHSV((loopcount % 360 / 1.0f), 1.0, 1.0);

    drawRect2D(top, 10, 10, 53, 53, colSelectedText);
    drawText(top, Vector2i(CharacterBitmaps::centered, 22), colSelectedText, "DOT");
    drawText(top, Vector2i(CharacterBitmaps::centered, 30), colSelectedText, "THE");
    drawText(top, Vector2i(CharacterBitmaps::centered, 38), colSelectedText, "LEDCUBE");

    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);

    bool charging = adcBattery.isCharging();

    auto battPercent = static_cast<float>(adcBattery.getPercentage()) / 100.0f;
    std::stringstream sstime;
    sstime << std::put_time(&tm, "%H:%M  %b %d %y");

    for (uint screenCounter = 4; screenCounter < 6; screenCounter++) {
        drawRect2D((ScreenNumber) screenCounter, 0, 0, CUBEMAXINDEX, 6, Color::black(), true, Color::black());
        drawRect2D((ScreenNumber) screenCounter, 0, CUBEMAXINDEX-6, CUBEMAXINDEX, CUBEMAXINDEX, Color::black(), true, Color::black());

        drawText((ScreenNumber) screenCounter, Vector2i(CharacterBitmaps::left, 58), Color::blue(), sstime.str() );
        drawText((ScreenNumber) screenCounter, Vector2i(CharacterBitmaps::left, 1), Color::blue(), hostname_);

        /* Draw battery indicator */
        int bat_lvl;
        Color bat_color, bat_outline;

        if (charging) {
            /* display scaning battery animation when charging */
            bat_color = Color::green();
            bat_outline = Color::white();
            bat_lvl = (loopcount >> 5) % 10;
        }
        else {
            bat_lvl = std::nearbyint(battPercent * 9.0f);
            uint8_t bat_color_g = std::nearbyint(battPercent * 255.0f);
            uint8_t bat_color_r = 255 - bat_color_g;
            bat_color = Color(bat_color_r, bat_color_g, 0);

            if (battPercent < 0.05) {
                /* blinking red/white battery outline when low on charge */
                if ((loopcount >> 6) & 1)
                    bat_outline = Color::red();
                else
                    bat_outline = Color::white();
            }
            else {
                bat_outline = Color::white();
            }
        }

        drawBitmap1bpp((ScreenNumber) screenCounter, Vector2i(CUBEMAXINDEX-11, 0), bat_outline, battery_indicator);
        if (bat_lvl)
            drawRect2D((ScreenNumber) screenCounter, CUBEMAXINDEX-10, 1, CUBEMAXINDEX-11+bat_lvl, 4, bat_color, true, bat_color);
        if (charging)
            drawText((ScreenNumber) screenCounter, Vector2i(CUBEMAXINDEX-7, 0), Color::white(), std::string("+") );
    }


    int sprite_idx;
    switch (lip_state_) {
    case 'A':
        sprite_idx = 0;
        break;
    case 'B':
        sprite_idx = 1;
        break;
    case 'C':
        sprite_idx = 2;
        break;
    case 'D':
        sprite_idx = 3;
        break;
    case 'E':
        sprite_idx = 4;
        break;
    case 'F':
        sprite_idx = 5;
        break;
    case 'G':
        sprite_idx = 6;
        break;
    case 'H':
        sprite_idx = 7;
        break;
    case 'X':
        sprite_idx = 8;
        break;
    default:
        sprite_idx = -1;
    }

    for (uint screenCounter = 0; screenCounter < 4; screenCounter++) {
        drawImage((ScreenNumber) screenCounter, Vector2i(0, 0), sprites_, Vector2i(0, 64*sprite_idx), Vector2i(64, 64));
    }


}


bool HBD::loop() {
    static unsigned loopcount = 0;
    static bool once;

    if (!once) {
        volume_ = (float)getVolume();
        accel_volume_ = 0.0;
        once = true;
    }

    switch (state_) {
    case MS_PLAYING:
        playingDraw(loopcount);
        break;
    case MS_END:
        if (thread_) {
            if (thread_->joinable())
                thread_->join();
        }
        return false;
    default:
        break;
    }

    auto val = joystickmngr.getAxis(2);
    if (abs(val) > JS_DEADZONE) {
        accel_volume_ += val * 0.03;
        volume_ += accel_volume_;
        if (volume_ > 100.0) volume_ = 100.0;
        if (volume_ < 0.0) volume_ = 0.0;
        int vol = std::ceil(volume_);
        setVolume(vol);

        std::stringstream ss;
        ss << "Volume: " << vol;
        for (uint screenCounter = 0; screenCounter < 6; screenCounter++) {
            drawRect2D((ScreenNumber) screenCounter, 0, 28, CUBEMAXINDEX, 36, Color::black(), true, Color::black());
            drawText((ScreenNumber) screenCounter, Vector2i(CharacterBitmaps::centered, 30), Color::green(), ss.str());
        }
    }
    else {
        accel_volume_ = 0.0;
    }

    joystickmngr.clearAllButtonPresses();
    render();
    loopcount++;
    return true;
}
