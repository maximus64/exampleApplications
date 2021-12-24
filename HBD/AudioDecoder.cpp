#include <string>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}
#include <stdio.h>
#include <stdexcept>
#include <vector>
#include "AudioDecoder.h"

static void dolog(void *foo, int level, const char *fmt, va_list ap)
{
    char buf[1024];
    vsnprintf(buf, 1024, fmt, ap);
    buf[1023] = 0;
    printf("%s", buf);
}

AudioDecoder::AudioDecoder(const char *fname) : stream_open_(false), pts_(0) {
    formatctx_ = NULL;
    codecctx_ = NULL;
    decoded_frame_ = NULL;
    codec_ = NULL;

    openStream(fname, 0);
}


int AudioDecoder::getSample(std::vector<int16_t> &buf, int samples) {
    AVPacket packet;
    size_t   data_size;
    int ret, total = 0;

    if (!stream_open_)
        return 0;

    while (samples)
    {
        if (buffered_samples_.size() == 0) {
            while (1) {
                ret = av_read_frame(formatctx_, &packet);
                if (ret == AVERROR_EOF) {
                    printf("%s: end of stream\n", __func__);
                    return 0;
                }
                else if (ret!=0) {
                    fprintf(stderr, "av_read_frame failed. ret=%d\n", ret);
                    exit(1);
                }
                if (packet.stream_index != stream_) {
                    av_packet_unref(&packet);
                    continue;
                }
                break;
            };

            /* send the packet with the compressed data to the decoder */
            ret = avcodec_send_packet(codecctx_, &packet);
            if (ret != 0) {
                fprintf(stderr, "Error submitting the packet to the decoder\n");
                exit(1);
            }
            av_packet_unref(&packet);

            /* read all the output frames (in general there may be any number of them */
            while (ret >= 0) {
                ret = avcodec_receive_frame(codecctx_, decoded_frame_);
                if (ret == AVERROR(EAGAIN)) {
                    break;
                }
                else if (ret == AVERROR_EOF) {
                    fprintf(stderr, "avcodec_receive_frame EOF? ret=%d\n", ret);
                    break;
                }
                else if (ret < 0) {
                    fprintf(stderr, "Error during decoding\n");
                    exit(1);
                }

                pts_ = decoded_frame_->pts;

                data_size = av_get_bytes_per_sample(codecctx_->sample_fmt);
                if (data_size < 0) {
                    /* This should not occur, checking just for paranoia */
                    fprintf(stderr, "Failed to calculate data size\n");
                    exit(1);
                }
                //for (int i = 0; i < decoded_frame_->nb_samples; i++) {
                    //for (int ch = 0; ch < codecctx_->channels; ch++)
                    //    fwrite(decoded_frame_->data[ch] + data_size*i, 1, data_size, outfile);
                //}
                // if(av_sample_fmt_is_planar(codecctx_->sample_fmt)) {
                //     buf.insert(buf.end(), decoded_frame_->data[0],  decoded_frame_->data[0]+ ( data_size * decoded_frame_->nb_samples));
                // }
                // else {
                //     for (int i = 0; i < decoded_frame_->nb_samples; i++) {
                //         const uint8_t *ptr = decoded_frame_->data[0] + i*codecctx_->channels*data_size;
                //         buf.insert(buf.end(), ptr, ptr + data_size);

                //     }
                // }

                {
                    uint8_t *dst_buf[1];
                    int src_nb_samples = decoded_frame_->nb_samples;
                    int src_rate = codecctx_->sample_rate;
                    int dst_rate = 48000;

                    int dst_nb_samples = av_rescale_rnd(swr_get_delay(resampler_, src_rate) +
                                        src_nb_samples, dst_rate, src_rate, AV_ROUND_UP);
                    size_t prev_offset = buffered_samples_.size();
                    buffered_samples_.resize(prev_offset + dst_nb_samples);
                    dst_buf[0] = (uint8_t*)(buffered_samples_.data() + prev_offset);

                    ret = swr_convert(resampler_, dst_buf, dst_nb_samples, (const uint8_t**)decoded_frame_->data, src_nb_samples);
                    if (ret < 0) {
                        fprintf(stderr, "Error while converting. ret=%d\n", ret);
                        exit(1);
                    }
                    buffered_samples_.resize(ret);
                }
            }
        }

        int max_chunk = std::min((int)(buffered_samples_.size()), samples);
        buf.insert(buf.end(), buffered_samples_.begin(), buffered_samples_.begin() + max_chunk);
        buffered_samples_.erase(buffered_samples_.begin(), buffered_samples_.begin() + max_chunk);

        samples -=  max_chunk;
        total += max_chunk;
    }

    return total;
}

/*
 * Print information about the input file and the used codec.
 */
static void printStreamInformation(const AVCodec* codec, const AVCodecContext* codecCtx, int audioStreamIndex) {
    fprintf(stderr, "Codec: %s\n", codec->long_name);
    if(codec->sample_fmts != NULL) {
        fprintf(stderr, "Supported sample formats: ");
        for(int i = 0; codec->sample_fmts[i] != -1; ++i) {
            fprintf(stderr, "%s", av_get_sample_fmt_name(codec->sample_fmts[i]));
            if(codec->sample_fmts[i+1] != -1) {
                fprintf(stderr, ", ");
            }
        }
        fprintf(stderr, "\n");
    }
    fprintf(stderr, "---------\n");
    fprintf(stderr, "Stream:        %7d\n", audioStreamIndex);
    fprintf(stderr, "Sample Format: %7s\n", av_get_sample_fmt_name(codecCtx->sample_fmt));
    fprintf(stderr, "Sample Rate:   %7d\n", codecCtx->sample_rate);
    fprintf(stderr, "Sample Size:   %7d\n", av_get_bytes_per_sample(codecCtx->sample_fmt));
    fprintf(stderr, "Channels:      %7d\n", codecCtx->channels);
    fprintf(stderr, "Planar:        %7d\n", av_sample_fmt_is_planar(codecCtx->sample_fmt));
    fprintf(stderr, "Float Output:  %7s\n", av_sample_fmt_is_planar(codecCtx->sample_fmt) ? "yes" : "no");
}

double AudioDecoder::getTimeStamp() {
    if (!stream_open_ || pts_ == AV_NOPTS_VALUE)
        return 0;

    // AVRational time_base = codecctx_->time_base;
    // AVRational time_base_q = {1,AV_TIME_BASE}; // AV_TIME_BASE_Q;
    // int64_t pts_time = av_rescale_q(pts_, time_base, time_base_q);
    return av_q2d(formatctx_->streams[stream_]->time_base) * pts_;
}

void AudioDecoder::openStream(const char *fname, float start_pos) {
    int ret;

    formatctx_ = NULL;

    //av_log_set_callback(dolog);
    //av_register_all();

    if (avformat_open_input(&formatctx_, fname, NULL, 0) != 0) {
        fprintf(stderr, "Failed to open input stream_\n");
        return;
    }

    if (avformat_find_stream_info(formatctx_, NULL) < 0) {
        fprintf(stderr, "Failed to get stream_ info\n");
        avformat_close_input(&formatctx_);
        return;
    }

    stream_=-1;
    for (int i=0; i < formatctx_->nb_streams; i++) {
        if (formatctx_->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO) {
            stream_=i;
            break;
        }
    }
    if (stream_==-1) {
        fprintf(stderr, "No audio stream available\n");
        avformat_close_input(&formatctx_);
        return;
    }

    codec_ = avcodec_find_decoder(formatctx_->streams[stream_]->codecpar->codec_id);
    if (!codec_) {
        fprintf(stderr, "Failed to find decoder for stream_ #%u\n", stream_);
        avformat_close_input(&formatctx_);
        return;
    }
    codecctx_ = avcodec_alloc_context3(codec_);
    if (!codecctx_) {
        fprintf(stderr, "Failed to allocate the decoder context for stream_ #%u\n", stream_);
        avformat_close_input(&formatctx_);
        return;
    }
    if (avcodec_parameters_to_context(codecctx_, formatctx_->streams[stream_]->codecpar) != 0) {
        fprintf(stderr, "Failed to copy decoder parameters to input decoder context "
                "for stream_ #%u\n", stream_);
        avcodec_free_context(&codecctx_);
        avformat_close_input(&formatctx_);
        return;
    }

    // codecctx_->request_sample_fmt = av_get_alt_sample_fmt(codecctx_->sample_fmt, 0);
    // printf("codecctx_->request_sample_fmt = %d\n", codecctx_->request_sample_fmt);

    if (avcodec_open2(codecctx_, codec_, NULL) != 0) {
        fprintf(stderr, "Failed to open avcodec\n");
        avcodec_close(codecctx_);
        avcodec_free_context(&codecctx_);
        avformat_close_input(&formatctx_);
        return;
    }

    // Print some intersting file information.
    printStreamInformation(codec_, codecctx_, stream_);

    printf("Alloc decoded frame\n");
    if (!(decoded_frame_ = av_frame_alloc())) {
        fprintf(stderr, "Could not allocate audio frame\n");
        avcodec_close(codecctx_);
        avcodec_free_context(&codecctx_);
        avformat_close_input(&formatctx_);
        return;
    }

    // if (start_pos) {
    //     av_seek_frame(formatctx_, -1, (int64_t)(start_pos * AV_TIME_BASE), 0);
    //     avcodec_flush_buffers(codecctx_);
    // }


    /* create resampler context */
    resampler_ = swr_alloc();
    if (!resampler_) {
        fprintf(stderr, "Could not allocate resampler context\n");
        av_frame_free(&decoded_frame_);
        avcodec_close(codecctx_);
        avcodec_free_context(&codecctx_);
        avformat_close_input(&formatctx_);
        return;
    }
    av_opt_set_channel_layout(resampler_, "in_channel_layout",  codecctx_->channel_layout, 0);\
    av_opt_set_channel_layout(resampler_, "out_channel_layout", AV_CH_LAYOUT_MONO,  0);
    av_opt_set_int(resampler_, "in_sample_rate", codecctx_->sample_rate, 0);
    av_opt_set_int(resampler_, "out_sample_rate", 48000, 0);
    av_opt_set_sample_fmt(resampler_, "in_sample_fmt", codecctx_->sample_fmt, 0);
    av_opt_set_sample_fmt(resampler_, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

    if (swr_init(resampler_) < 0) {
        fprintf(stderr, "Failed to initialize the resampling context\n");
        swr_free(&resampler_);
        av_frame_free(&decoded_frame_);
        avcodec_close(codecctx_);
        avcodec_free_context(&codecctx_);
        avformat_close_input(&formatctx_);
        return;
    }

    stream_open_ = true;
}


AudioDecoder::~AudioDecoder() {
    if (stream_open_) {
        swr_free(&resampler_);
        avcodec_close(codecctx_);
        avcodec_free_context(&codecctx_);
        avformat_close_input(&formatctx_);
        av_frame_free(&decoded_frame_);
    }
}
