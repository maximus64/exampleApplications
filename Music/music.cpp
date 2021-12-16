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

Music::Music() : CubeApplication(60), amplitute_(0), db_mag_({}), db_smooth_({}), maxVal_(0), minVal_(0) {
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

#define NUM_SAMPLE 256
    buf.reserve(NUM_SAMPLE);
    double *mag;
    double *in;
    fftw_complex *out;
    fftw_plan p;

    in = fftw_alloc_real(NUM_SAMPLE);
    out = fftw_alloc_complex(NUM_SAMPLE / 2 + 1);

    p = fftw_plan_dft_r2c_1d( NUM_SAMPLE, in, out, FFTW_MEASURE );

    while (getAppState() == AppState::running) {
        auto count = ad.getSample(buf, 256);
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
            db_mag_[i] = hypot(out[i][0], out[i][1]);
            db_mag_[i] = (db_mag_[i] == 0.0) ? 0.0 : 20.0 * log10(db_mag_[i]);
            //db_mag_[i] =  10.0 * log10(out[i][0] * out[i][0] +  out[i][1] * out[i][1]);
        }

        buf.clear();
        //std::this_thread::sleep_for (std::chrono::seconds(1));
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
    Color bg_color;
    static const double smooth_factor = 0.8;
    bool firstMinDone = false;
    int i;
    int bars[128];
    double curr_min = 0;

    clear();

    // intensity -= 0.05f;
    // if (amp > intensity)
    //     intensity = amp;

    //bg_color.fromHSV(0, 1, (loopcount % 10) / 10.0f);
    bg_color.fromHSV(loopcount % 360, 1, intensity);
    //fillAll(bg_color);

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

    const double min_smooth_factor = 0.99;
    minVal_ = minVal_ * min_smooth_factor + (( 1.0- min_smooth_factor) * curr_min);

    double range = maxVal_ - minVal_;
    double scale_factor = range + 0.00001; // avoid div. by zero

    for (i = 0; i < 128; i++) {
        //db_mag_[i] / 2.0;
        double height =  64.0f * (db_smooth_[i] - minVal_) / scale_factor;
        bars[i] = (int)height;
    }

    for (i = 0; i < 64; i++) {
        int h = bars[i];
        if (h > 0)
            drawLine2D(ScreenNumber::front, 
                    i,63,
                    i,64-std::min(h, 63),
                    bg_color);
    }
    for (i = 0; i < 64; i++) {
        int h = bars[64+i];
        if (h > 0)
            drawLine2D(ScreenNumber::right, 
                    i,63,
                    i,64-std::min(h, 63),
                    bg_color);
    }
    for (i = 0; i < 64; i++) {
        int h = bars[64-i];
        if (h > 0)
            drawLine2D(ScreenNumber::left, 
                    i,63,
                    i,64-std::min(h, 63),
                    bg_color);
    }
    for (i = 0; i < 64; i++) {
        int h = bars[64+64-i];
        if (h > 0)
            drawLine2D(ScreenNumber::back, 
                    i,63,
                    i,64-std::min(h, 63),
                    bg_color);
    }

//         drawLine3D(Vector3i(0,0,CUBESIZE-loopcount%CUBESIZE),Vector3i(CUBESIZE,0,CUBESIZE-loopcount%CUBESIZE), Color::red());
//         drawLine3D(Vector3i(loopcount%CUBESIZE,0,CUBESIZE),Vector3i(loopcount%CUBESIZE,0,0), Color::blue());
// //    }
//     drawText(ScreenNumber::front, Vector2i(CharacterBitmaps::centered, CharacterBitmaps::centered), Color::white(), "Screen 0 front");
//     drawText(ScreenNumber::right, Vector2i(CharacterBitmaps::centered, CharacterBitmaps::centered), Color::white(), "Screen 1 right");
//     drawText(ScreenNumber::back, Vector2i(CharacterBitmaps::centered, CharacterBitmaps::centered), Color::white(), "Screen 2 back");
//     drawText(ScreenNumber::left, Vector2i(CharacterBitmaps::centered, CharacterBitmaps::centered), Color::white(), "Screen 3 left");
//     drawText(ScreenNumber::top, Vector2i(CharacterBitmaps::centered, CharacterBitmaps::centered), Color::white(), "Screen 4 top");
//     drawText(ScreenNumber::bottom, Vector2i(CharacterBitmaps::centered, CharacterBitmaps::centered), Color::white(), "Screen 5 bottom");
    

    render();
    loopcount++;
    return true;
}
