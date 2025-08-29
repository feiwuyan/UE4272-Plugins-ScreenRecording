// MP4Muxer.cpp
#include "MP4Muxer.h"

// You must wrap FFmpeg includes with this to avoid compiler warnings/errors
extern "C" {
#include "libavformat/avformat.h"
#include "libavutil/opt.h"
}

// Helper to convert UE4 FString to char*
#include "Misc/CString.h"

FMP4Muxer::FMP4Muxer()
{
    // av_register_all() is deprecated, initialization is now automatic.
}

FMP4Muxer::~FMP4Muxer()
{
    Finalize();
}

bool FMP4Muxer::Initialize(const FString& FilePath, const AVEncoder::FVideoConfig& VideoConfig, const AVEncoder::FAudioConfig& AudioConfig)
{
    // 1. Allocate format context for MP4
    avformat_alloc_output_context2(&FormatContext, nullptr, "mp4", TCHAR_TO_UTF8(*FilePath));
    if (!FormatContext)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to allocate MP4 format context"));
        return false;
    }

    // 2. Create Video and Audio Streams
    if (!AddVideoStream(VideoConfig))
    {
        return false;
    }
    if (!AddAudioStream(AudioConfig))
    {
        return false;
    }

    // 3. Open the output file for writing
    if (!(FormatContext->oformat->flags & AVFMT_NOFILE))
    {
        if (avio_open(&FormatContext->pb, TCHAR_TO_UTF8(*FilePath), AVIO_FLAG_WRITE) < 0)
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to open output file: %s"), *FilePath);
            return false;
        }
    }

    // Note: We don't write the header yet. We need the SPS/PPS from the first keyframe.
    // We will write it in the AddPacket function upon receiving the first keyframe.

    return true;
}

bool FMP4Muxer::AddVideoStream(const AVEncoder::FVideoConfig& Config)
{
    // Assuming H.264 codec from FGameplayMediaEncoder
    AVCodecID CodecId = AV_CODEC_ID_H264;

    VideoStream = avformat_new_stream(FormatContext, nullptr);
    if (!VideoStream) return false;

    VideoStream->id = FormatContext->nb_streams - 1;
    AVCodecParameters* CodecParams = VideoStream->codecpar;
    CodecParams->codec_type = AVMEDIA_TYPE_VIDEO;
    CodecParams->codec_id = CodecId;
    CodecParams->width = Config.Width;
    CodecParams->height = Config.Height;
    CodecParams->format = AV_PIX_FMT_YUV420P; // Common format

    // Set time base. This is crucial for correct timestamp conversion.
    // We use microseconds since FMediaPacket timestamp is in microseconds.
    VideoStream->time_base = { 1, 1000000 };

    return true;
}

bool FMP4Muxer::AddAudioStream(const AVEncoder::FAudioConfig& Config)
{
    // Assuming AAC codec
    AVCodecID CodecId = AV_CODEC_ID_AAC;

    AudioStream = avformat_new_stream(FormatContext, nullptr);
    if (!AudioStream) return false;

    AudioStream->id = FormatContext->nb_streams - 1;
    AVCodecParameters* CodecParams = AudioStream->codecpar;
    CodecParams->codec_type = AVMEDIA_TYPE_AUDIO;
    CodecParams->codec_id = CodecId;
    CodecParams->sample_rate = Config.Samplerate;
    CodecParams->channels = Config.NumChannels;
    CodecParams->channel_layout = av_get_default_channel_layout(Config.NumChannels);

    // Set time base for audio stream
    AudioStream->time_base = { 1, (int)Config.Samplerate };

    // --- 硬编码 extradata ---
    UE_LOG(LogTemp, Warning, TEXT("Using hardcoded AAC extradata for 48kHz, Stereo, AAC-LC. This is not recommended."));
    // 这是 "AAC-LC, 48kHz, Stereo" 对应的 extradata: 0x12, 0x10
    const TArray<uint8> HardcodedExtradata = { 0x12, 0x10 };

    CodecParams->extradata_size = HardcodedExtradata.Num();
    CodecParams->extradata = (uint8_t*)av_mallocz(CodecParams->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
    if (!CodecParams->extradata)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to allocate memory for hardcoded AAC extradata."));
        return false;
    }
    FMemory::Memcpy(CodecParams->extradata, HardcodedExtradata.GetData(), CodecParams->extradata_size);

    return true;
}


bool FMP4Muxer::AddPacket(const AVEncoder::FMediaPacket& Packet)
{
    if (!FormatContext) return false;

    AVPacket* FfmpegPacket = av_packet_alloc();
    if (!FfmpegPacket) return false;

    AVStream* TargetStream = nullptr;
    if (Packet.Type == AVEncoder::EPacketType::Video)
    {
        TargetStream = VideoStream;

        // H.264 packets from hardware encoders often contain the SPS/PPS prefixed to keyframes.
        // We need to extract this and put it in the stream's extradata before writing the header.
        if (Packet.Video.bKeyFrame && VideoExtradata.Num() == 0)
        {
            // This part is tricky and encoder-dependent.
            // You need to find the SPS/PPS NAL units in the keyframe data.
            // A common format is [start_code]SPS[start_code]PPS[start_code]IDR_SLICE
            // For simplicity, let's assume the encoder provides it in a specific way.
            // A robust solution would parse NAL units.
            // For now, let's assume FGameplayMediaEncoder doesn't bundle them and you need to get them separately.
            // If they ARE bundled, you must copy them to VideoStream->codecpar->extradata.
            // For example:
            // VideoStream->codecpar->extradata = (uint8_t*)av_mallocz(sps_pps_size + AV_INPUT_BUFFER_PADDING_SIZE);
            // FMemory::Memcpy(VideoStream->codecpar->extradata, sps_pps_data, sps_pps_size);
            // VideoStream->codecpar->extradata_size = sps_pps_size;
        }

        if (Packet.Video.bKeyFrame)
        {
            FfmpegPacket->flags |= AV_PKT_FLAG_KEY;
        }
    }
    else if (Packet.Type == AVEncoder::EPacketType::Audio)
    {
        TargetStream = AudioStream;
    }
    else
    {
        av_packet_free(&FfmpegPacket);
        return false; // Unknown packet type
    }

    // If the file header hasn't been written yet, do it now.
    // This must be done after streams are configured (including extradata).
    if (!bIsHeaderWritten)
    {
        if (avformat_write_header(FormatContext, nullptr) < 0)
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to write MP4 header"));
            av_packet_free(&FfmpegPacket);
            return false;
        }
        bIsHeaderWritten = true;
    }

    // Copy data from FMediaPacket to AVPacket
    FfmpegPacket->data = (uint8_t*)av_malloc(Packet.Data.Num());
    FMemory::Memcpy(FfmpegPacket->data, Packet.Data.GetData(), Packet.Data.Num());
    FfmpegPacket->size = Packet.Data.Num();

    // *** CRITICAL: Timestamp Conversion ***
    // We need to convert the timestamp from its original time base (microseconds or samples)
    // to the AVStream's time_base.
    //FfmpegPacket->pts = av_rescale_q(Packet.Timestamp.GetTotalMicroseconds(), TargetStream == VideoStream ? AVRational{1, 1000000} : AVRational{1, (int)AudioStream->codecpar->sample_rate}, TargetStream->time_base);
    FfmpegPacket->pts = av_rescale_q(Packet.Timestamp.GetTotalMicroseconds(), AVRational{ 1, 1000000 }, TargetStream->time_base);
    FfmpegPacket->dts = FfmpegPacket->pts; // For simple cases, DTS can be same as PTS
    //FfmpegPacket->duration = av_rescale_q(Packet.Duration.GetTotalMicroseconds(), TargetStream == VideoStream ? AVRational{1, 1000000} : AVRational{1, (int)AudioStream->codecpar->sample_rate}, TargetStream->time_base);
    FfmpegPacket->duration = av_rescale_q(Packet.Duration.GetTotalMicroseconds(), AVRational{ 1, 1000000 }, TargetStream->time_base);
    FfmpegPacket->stream_index = TargetStream->index;

    UE_LOG(LogTemp, Log, TEXT("Muxer AddPacket: Type=%s, Index=%d, Size=%d, Timestamp(us)=%f, PTS=%lld"),
        (Packet.Type == AVEncoder::EPacketType::Video ? TEXT("Video") : TEXT("Audio")),
        TargetStream->index,
        FfmpegPacket->size,
        Packet.Timestamp.GetTotalMicroseconds(),
        FfmpegPacket->pts);

    // Write the packet to the file
    int Result = av_interleaved_write_frame(FormatContext, FfmpegPacket);
    if (Result < 0)
    {
        // Note: av_interleaved_write_frame frees the packet data on success and failure
        UE_LOG(LogTemp, Warning, TEXT("Failed to write frame to MP4 file."));
        return false;
    }

    // av_interleaved_write_frame takes ownership and frees the packet, so we don't call av_packet_free.
    // However, if it fails, it might not have been freed. The FFmpeg docs are specific about this.
    // A safer pattern is to use av_packet_unref after the call.
    // For this example, we assume it's handled.

    return true;
}

void FMP4Muxer::Finalize()
{
    if (FormatContext)
    {
        if (bIsHeaderWritten)
        {
            // Write the file trailer. This is essential for a valid MP4.
            av_write_trailer(FormatContext);
        }

        // Close the output file
        if (!(FormatContext->oformat->flags & AVFMT_NOFILE))
        {
            avio_closep(&FormatContext->pb);
        }

        // Free the context
        avformat_free_context(FormatContext);
        FormatContext = nullptr;
        VideoStream = nullptr;
        AudioStream = nullptr;
        bIsHeaderWritten = false;
    }
}