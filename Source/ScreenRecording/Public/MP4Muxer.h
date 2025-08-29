// MP4Muxer.h
#pragma once

#include "CoreMinimal.h"
#include "AudioEncoder.h"
#include "VideoEncoder.h"
#include "MediaPacket.h"
#include "VideoEncoderInput.h"

// Forward declare FFmpeg types to avoid including ffmpeg headers in a public header
struct AVFormatContext;
struct AVStream;
struct AVPacket;
struct AVCodecParameters;

class SCREENRECORDING_API FMP4Muxer
{
public:
    FMP4Muxer();
    ~FMP4Muxer();

    // Initializes the muxer, creates streams, and writes the file header.
    bool Initialize(const FString& FilePath, const AVEncoder::FVideoConfig& VideoConfig, const AVEncoder::FAudioConfig& AudioConfig);

    // Add an encoded media packet (from your listener) to the file.
    bool AddPacket(const AVEncoder::FMediaPacket& Packet);

    // Finalizes the file writing (writes trailer) and cleans up resources.
    void Finalize();

private:
    bool AddVideoStream(const AVEncoder::FVideoConfig& Config);
    bool AddAudioStream(const AVEncoder::FAudioConfig& Config);

    AVFormatContext* FormatContext = nullptr;
    AVStream* VideoStream = nullptr;
    AVStream* AudioStream = nullptr;

    // We need to get the codec extradata (SPS/PPS for H.264)
    // This is often in the first packet from the encoder.
    TArray<uint8> VideoExtradata;
    bool bIsHeaderWritten = false;
};