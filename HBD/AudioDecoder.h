#ifndef AUDIO_DECODER_H_
#define AUDIO_DECODER_H_

#include <string>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

class AudioDecoder {
public:
    AudioDecoder(const char *fname);
    ~AudioDecoder();
    int getSample(std::vector<int16_t> &buf, int samples);
    double getTimeStamp();
private:
    void openStream(const char *fname, float start_pos);
    AVFormatContext *formatctx_;
    AVCodecContext *codecctx_;
    AVFrame *decoded_frame_;
    const AVCodec *codec_;
    struct SwrContext *resampler_;
    int stream_;
    std::vector<int16_t> buffered_samples_;
    bool stream_open_;
    int64_t pts_;
};


#endif
