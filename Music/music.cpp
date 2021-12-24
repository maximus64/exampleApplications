#include "music.h"
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
#include <fftw3.h>
#include <filesystem>

#define JS_DEADZONE 0.5

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
}

Music::Music() : 
    CubeApplication(60),
    joystickmngr(8),
    amplitute_(0),
    db_mag_{},
    db_smooth_{},
    maxVal_(0),
    minVal_(50.0),
    line_hue_ (0),
    state_(MS_SELECT) {
    static const double aWeightDecibels[16] = {
        -24.22, -13.37, -8, -3.36,
        -1.33, 0.00, 0.70, 0.91,
        0.70, -0.07, -0.40, -0.84,
        -1.60, -2.30, -3.15, -3.85,
    };

    for (int i = 0; i < 128; i++) {
        int idxa = i / 8;
        int idxb = idxa >= 15 ? 15 : idxa + 1;
        double t = (i % 8) / 8.0;

        db_weight_[i] = aWeightDecibels[idxa] + t * (aWeightDecibels[idxb] - aWeightDecibels[idxa]);
    }

    constexpr std::string_view searchDirectory = "/nvram/music";
    try {
        for (const auto &p : std::filesystem::directory_iterator(searchDirectory)) {
            songs_list_.push_back(SongInfo(std::string(p.path().stem()), std::string(p.path())));
        }
    }
    catch (...) {
    }

    char hostname[20];
    gethostname(hostname, sizeof(hostname));
    hostname_ = std::string(hostname);
}

void Music::decodeLoop() {
    int err;
    std::vector<int16_t> buf;

    AudioDecoder ad(songs_list_[song_idx_].path.c_str());

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
    double *mag;
    double *in;
    fftw_complex *out;
    fftw_plan p;

    in = fftw_alloc_real(NUM_SAMPLE);
    out = fftw_alloc_complex(NUM_SAMPLE / 2 + 1);

    p = fftw_plan_dft_r2c_1d( NUM_SAMPLE, in, out, FFTW_MEASURE );

    while (getAppState() == AppState::running) {
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

        int16_t max = 0;
        for (int i = 0; i < count; i++) {
            /* convert sample to double for FFT */
            in[i] = (double)buf[i];

            /* find max amptitude */
            if (buf[i] > max)
                max = buf[i];
        }
        amplitute_ = max / 32768.0f;

        fftw_execute(p);

        for ( int i = 0; i < 128; i++ ) {
            // db_mag_[i] = hypot(out[i][0], out[i][1]);
            // db_mag_[i] = (db_mag_[i] == 0.0) ? 0.0 : 20.0 * log10(db_mag_[i]);
            db_mag_[i] =  10.0 * log10(out[i][0] * out[i][0] +  out[i][1] * out[i][1]);
            db_mag_[i] += db_weight_[i];

            /* clamp value */
            if (db_mag_[i] < 0) db_mag_[i] = 0.0;
            if (db_mag_[i] > 150.0) db_mag_[i] = 150.0;
        }

        buf.clear();
    }

    fftw_destroy_plan(p);
    fftw_free(in);
    fftw_free(out);
    snd_pcm_drop(handle);
    snd_pcm_close(handle);
}

void Music::playingDraw(unsigned loopcount) {
    constexpr float intensity = 1.0f;
    float amp = abs(amplitute_);
    Color line_color;
    static const double smooth_factor = 0.6;//0.8;
    bool firstMinDone = false;
    int i;
    int bars[128];
    double curr_min = 0;
    float hue = line_hue_;
    constexpr float hue_step = 1.40625;

    fade(.5);

    for (i = 0; i < 128; i++) {
        // Smooth using exponential moving average
        db_smooth_[i] = (smooth_factor) * db_smooth_[i] + ((1.0 - smooth_factor) * db_mag_[i]);

        // Find max and min values ever displayed across whole spectrum
        if (db_smooth_[i] > maxVal_) {
            maxVal_ = db_smooth_[i];
        }
        if (!firstMinDone || (db_smooth_[i] < curr_min)) {
            curr_min = db_smooth_[i];
            firstMinDone = true;
        }
    }

    const double min_smooth_factor = 0.80;
    minVal_ = minVal_ * min_smooth_factor + (( 1.0- min_smooth_factor) * curr_min);

    double range = maxVal_ - minVal_;
    double scale_factor = range + 0.00001; // avoid div. by zero

    for (i = 0; i < 128; i++) {
        //db_mag_[i] / 2.0;
        double height =  64.0f * (db_smooth_[i] - minVal_) / scale_factor;
        bars[i] = (int)height;
    }

    for (i = 0; i < 64; i++) {
        line_color.fromHSV(hue, 1, intensity);
        hue += hue_step;
        int h = bars[i];
        if (h > 0) {
            drawLine2D(ScreenNumber::front, 
                    i,63,
                    i,64-std::min(h, 63),
                    line_color);
        }
    }
    for (i = 0; i < 64; i++) {
        line_color.fromHSV(hue, 1, intensity);
        hue += hue_step;
        int h = bars[64+i];
        if (h > 0) {
            drawLine2D(ScreenNumber::right, 
                    i,63,
                    i,64-std::min(h, 63),
                    line_color);
        }
    }
    for (i = 0; i < 64; i++) {
        line_color.fromHSV(hue, 1, intensity);
        hue += hue_step;
        int h = bars[i];
        if (h > 0) {
            drawLine2D(ScreenNumber::back, 
                    i,63,
                    i,64-std::min(h, 63),
                    line_color);
        }
    }
    for (i = 0; i < 64; i++) {
        line_color.fromHSV(hue, 1, intensity);
        hue += hue_step;
        int h = bars[64+i];
        if (h > 0) {
            drawLine2D(ScreenNumber::left, 
                    i,63,
                    i,64-std::min(h, 63),
                    line_color);
        }
    }

    i = 0;
    for (float r = 0.0; i < 128; r+= M_PI *2.0 / 128, i++) {
        double normal = abs((db_smooth_[127-i] - minVal_) / scale_factor);
        double height =  32.0f * normal;
        double x = height * cos(r);
        double y = height * sin(r);
        line_color.fromHSV(360 - r*(180.0/M_PI) + line_hue_, 1, 1);
        drawLine2D(ScreenNumber::top, 
                    CUBECENTER,CUBECENTER,
                    CUBECENTER-x,CUBECENTER-y,
                    line_color);
    }

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

    line_hue_ += 1.0f;
}

void Music::selectDraw(unsigned loopcount) {
    static int sel = 0;
    static int animationOffset = 0;
    static int last_sel = sel;
    static unsigned scoll_cnt;
    Color colSelectedText;
    colSelectedText.fromHSV((loopcount % 360 / 1.0f), 1.0, 1.0);

    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);

    bool charging = adcBattery.isCharging();

    if (joystickmngr.getButtonPress(0) && sel < songs_list_.size()) {
        printf("Select Song: %s\n", songs_list_[sel].path.c_str());
        song_idx_ = sel;
        state_ = MS_PLAYING;
        thread_ = std::make_unique<std::thread> ( [this](){ decodeLoop(); } );
        clear();
        return;
    }

    sel += (int) joystickmngr.getAxisPress(1);
    if (sel < 0)
        sel = songs_list_.size() - 1;
    else
        sel %= songs_list_.size();

    if(last_sel != sel){
        scoll_cnt = 0;
        animationOffset = (sel-last_sel)*7;
    }
    last_sel = sel;
    animationOffset *= 0.85;

    clear();

    constexpr int max_char = CUBESIZE / CharacterBitmaps::fontWidth;

    for (int i = 0; i < songs_list_.size(); i++) {
        int yPos = 29 + ((i - sel) * 7) + animationOffset;
        Color textColor = songs_list_.at(i).color;
        if (i == sel)
            textColor = colSelectedText;
        for (int screenCounter = 0; screenCounter < 4; screenCounter++) {
            if (yPos < CUBEMAXINDEX && yPos > 0) {
                std::string disp_name = songs_list_.at(i).name;
                if (disp_name.length() > max_char) {
                    if (i == sel) {
                        /*FIX ME! shitty text scoll */
                        disp_name += " - ";
                        int char_pos = (scoll_cnt >> 4) % disp_name.length();
                        disp_name = disp_name.substr(char_pos) + disp_name.substr(0, char_pos);
                        disp_name = disp_name.substr(0, max_char);
                    }
                    disp_name = disp_name.substr(0, max_char);
                }
                drawText((ScreenNumber) screenCounter, Vector2i(CharacterBitmaps::centered, yPos), textColor, disp_name);
            }
        }
    }

    auto battPercent = static_cast<float>(adcBattery.getPercentage()) / 100.0f;
    std::stringstream sstime;
    sstime << std::put_time(&tm, "%H:%M  %b %d %y");

    for (uint screenCounter = 0; screenCounter < 6; screenCounter++) {
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

    for (uint screenCounter = 4; screenCounter < 6; screenCounter++) {
        drawText(top, Vector2i(CharacterBitmaps::centered, 30), colSelectedText,
            songs_list_.size() ? "Select Song" : "No music found");
    }

    scoll_cnt++;
}


bool Music::loop() {
    static unsigned loopcount = 0;
    static bool once;

    if (!once) {
        volume_ = (float)getVolume();
        accel_volume_ = 0.0;
        once = true;
    }

    switch (state_) {
    case MS_SELECT:
        selectDraw(loopcount);
        break;
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
