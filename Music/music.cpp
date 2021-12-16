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

Music::Music() : CubeApplication(60), amplitute_(0), db_mag_{}, db_smooth_{}, maxVal_(0), minVal_(50.0) {
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

    thread_ = std::unique_ptr<std::thread> (
        new std::thread(&Music::playerLoop, this)
    );
}

void Music::playerLoop() {
    int err;
    std::vector<int16_t> buf;

    AudioDecoder ad("/root/test.mp3");

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
        if (count <= 0)
            break;
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

bool Music::loop() {
    static int loopcount = 0;
    static float intensity = 1.0f;
    float amp = abs(amplitute_);
    Color line_color;
    static const double smooth_factor = 0.6;//0.8;
    bool firstMinDone = false;
    int i;
    int bars[128];
    double curr_min = 0;
    float hue = line_hue_;
    const float hue_step = 1.40625;


    //clear();
    fade(.5);

    // intensity -= 0.05f;
    // if (amp > intensity)
    //     intensity = amp;

    //line_color.fromHSV(0, 1, (loopcount % 10) / 10.0f);
    //fillAll(line_color);

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
        int h = bars[64+64-i];
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
        int h = bars[64-i];
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

    render();
    loopcount++;
    line_hue_ += 1.5f;
    return true;
}
