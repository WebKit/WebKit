/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#import "config.h"
#import "RTCVideoDecoderVTBAV1.h"

#if USE(LIBWEBRTC)

#import "Logging.h"
#import <span>
#import <webrtc/modules/video_coding/include/video_error_codes.h>
#import <wtf/BlockPtr.h>
#import <wtf/CheckedArithmetic.h>
#import <wtf/FastMalloc.h>
#import <wtf/RetainPtr.h>

#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>

using namespace WebCore;

typedef struct OpaqueVTDecompressionSession*  VTDecompressionSessionRef;
typedef void (*VTDecompressionOutputCallback)(void * decompressionOutputRefCon, void * sourceFrameRefCon, OSStatus status, VTDecodeInfoFlags infoFlags, CVImageBufferRef imageBuffer, CMTime presentationTimeStamp, CMTime presentationDuration);

class BitReader {
public:
    explicit BitReader(std::span<const uint8_t> data)
        : m_data(data)
    {
    }

    std::optional<uint64_t> read(size_t);

private:
    std::optional<bool> readBit();

    std::span<const uint8_t> m_data;
    size_t m_index { 0 };
    uint8_t m_currentByte { 0 };
    size_t m_remainingBits { 0 };
};

std::optional<uint64_t> BitReader::read(size_t bits)
{
    // FIXME: We should optimize this routine.
    size_t value = 0;
    do {
        auto bit = readBit();
        if (!bit)
            return { };
        value = (value << 1) | (*bit ? 1 : 0);
        --bits;
    } while (bits);
    return value;
}

std::optional<bool> BitReader::readBit()
{
    if (!m_remainingBits) {
        if (m_index >= m_data.size())
            return { };
        m_currentByte = m_data[m_index++];
        m_remainingBits = 8;
    }

    bool value = m_currentByte & 0x80;
    --m_remainingBits;
    m_currentByte = m_currentByte << 1;
    return value;
}

static size_t readULEBSize(std::span<const uint8_t> data, size_t& index)
{
    size_t value = 0;
    for (size_t cptr = 0; cptr < 8; ++cptr) {
        if (index >= data.size())
            return 0;

        uint8_t dataByte = data[index++];
        uint8_t decodedByte = dataByte & 0x7f;
        value |= decodedByte << (7 * cptr);
        if (value >= std::numeric_limits<uint32_t>::max())
            return 0;
        if (!(dataByte & 0x80))
            break;
    }
    return value;
}

static std::optional<std::pair<std::span<const uint8_t>, std::span<const uint8_t>>> getSequenceHeaderOBU(std::span<const uint8_t> data)
{
    size_t index = 0;
    do {
        if (index >= data.size())
            return { };

        auto startIndex = index;
        auto value = data[index++];
        if (value >> 7)
            return { };
        auto headerType = value >> 3;
        bool hasPayloadSize = value & 0x02;
        if (!hasPayloadSize)
            return { };

        bool hasExtension = value & 0x04;
        if (hasExtension)
            ++index;

        Checked<size_t> payloadSize = readULEBSize(data, index);
        if (index + payloadSize >= data.size())
            return { };

        if (headerType == 1) {
            std::span<const uint8_t> fullObu { data.data() + startIndex, payloadSize + index - startIndex };
            std::span<const uint8_t> obuData { data.data() + index, payloadSize };
            return std::make_pair(fullObu, obuData);
        }

        index += payloadSize;
    } while (true);
    return { };
}

struct ParsedSequenceHeaderParameters {
    int32_t height { 0 };
    int32_t width { 0 };

    uint8_t profile { 0 };
    uint8_t level { 0 };

    uint8_t high_bitdepth { 0 };
    uint8_t twelve_bit { 0 };
    uint8_t chroma_type { 0 };
};

static std::optional<ParsedSequenceHeaderParameters> parseSequenceHeaderOBU(std::span<const uint8_t> data)
{
    ParsedSequenceHeaderParameters parameters;

    BitReader bitReader(data);
    std::optional<size_t> value;

    // Read three bits, profile
    value = bitReader.read(3);
    if (!value)
        return { };
    parameters.profile = *value;

    // Read one bit, still picture
    value = bitReader.read(1);
    if (!value)
        return { };

    // Read one bit, hdr still picture
    value = bitReader.read(1);
    if (!value)
        return { };
    // We only support hdr still picture = 0 for now.
    if (*value)
        return { };

    parameters.high_bitdepth = 0;
    parameters.twelve_bit = 0;
    parameters.chroma_type = 3;

    // Read one bit, timing info
    value = bitReader.read(1);
    if (!value)
        return { };
    // We only support no timing info for now.
    if (*value)
        return { };

    // Read one bit, display mode
    value = bitReader.read(1);
    if (!value)
        return { };

    // Read 5 bits, operating_points_cnt_minus_1
    value = bitReader.read(5);
    if (!value)
        return { };
    // We only support operating_points_cnt_minus_1 = 0 for now.
    if (*value)
        return { };

    // Read 12 bits, operating_point_idc
    value = bitReader.read(12);
    if (!value)
        return { };

    // Read 5 bits, level
    value = bitReader.read(5);
    if (!value)
        return { };
    parameters.level = *value;

    // If level >= 4.0, read one bit
    if (parameters.level > 7) {
        value = bitReader.read(1);
        if (!value)
            return { };
    }

    // Read width num bits
    value = bitReader.read(4);
    if (!value)
        return { };
    size_t widthNumBits = *value + 1;

    // Read height num bits
    value = bitReader.read(4);
    if (!value)
        return { };
    size_t heightNumBits = *value + 1;

    // Read width according with num bits
    value = bitReader.read(widthNumBits);
    if (!value)
        return { };
    parameters.width = *value + 1;

    // Read height according with num bits
    value = bitReader.read(heightNumBits);
    if (!value)
        return { };
    parameters.height = *value + 1;

    return parameters;
}

static RetainPtr<CMVideoFormatDescriptionRef> computeAV1InputFormat(std::span<const uint8_t> data, int32_t width, int32_t height)
{
    auto sequenceHeaderData = getSequenceHeaderOBU(data);
    if (!sequenceHeaderData)
        return { };

    auto parameters = parseSequenceHeaderOBU(sequenceHeaderData->second);
    if (!parameters)
        return { };

    if (width && parameters->width != width)
        return { };
    if (height && parameters->height != height)
        return { };

    auto fullOBUHeader = sequenceHeaderData->first;

    constexpr size_t VPCodecConfigurationContentsSize = 4;
    size_t cfDataSize = VPCodecConfigurationContentsSize + fullOBUHeader.size();
    auto cfData = adoptCF(CFDataCreateMutable(kCFAllocatorDefault, cfDataSize));
    CFDataIncreaseLength(cfData.get(), cfDataSize);
    uint8_t* header = CFDataGetMutableBytePtr(cfData.get());

    header[0] = 129;
    header[1] = (parameters->profile << 5) | parameters->level;
    header[2] = (parameters->high_bitdepth << 6) | (parameters->twelve_bit << 5) | (parameters->chroma_type << 2);
    header[3] = 0;

    memcpy(header + 4, fullOBUHeader.data(), fullOBUHeader.size());

    auto configurationDict = @{ @"av1C": (__bridge NSData *)cfData.get() };
    auto extensions = @{ (__bridge NSString *)PAL::kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms: configurationDict };

    CMVideoFormatDescriptionRef formatDescription = nullptr;
    // Use kCMVideoCodecType_AV1 once added to CMFormatDescription.h
    if (noErr != PAL::CMVideoFormatDescriptionCreate(kCFAllocatorDefault, 'av01', parameters->width, parameters->height, (__bridge CFDictionaryRef)extensions, &formatDescription))
        return { };

    return adoptCF(formatDescription);
}

struct RTCFrameDecodeParams {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    BlockPtr<void(CVPixelBufferRef, long long, long long, bool)> callback;
    int64_t timestamp { 0 };
};

@interface RTCVideoDecoderVTBAV1 ()
- (void)setError:(OSStatus)error;
@end

static RetainPtr<CMSampleBufferRef> av1BufferToCMSampleBuffer(std::span<const uint8_t> buffer, CMVideoFormatDescriptionRef videoFormat)
{
    CMBlockBufferRef newVlockBuffer;
    if (auto error = PAL::CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, NULL, buffer.size(), kCFAllocatorDefault, NULL, 0, buffer.size(), kCMBlockBufferAssureMemoryNowFlag, &newVlockBuffer)) {
        RELEASE_LOG_ERROR(WebRTC, "AV1BufferToCMSampleBuffer CMBlockBufferCreateWithMemoryBlock failed with: %d", error);
        return nullptr;
    }
    auto blockBuffer = adoptCF(newVlockBuffer);

    if (auto error = PAL::CMBlockBufferReplaceDataBytes(buffer.data(), blockBuffer.get(), 0, buffer.size())) {
        RELEASE_LOG_ERROR(WebRTC, "AV1BufferToCMSampleBuffer CMBlockBufferReplaceDataBytes failed with: %d", error);
        return nullptr;
    }

    CMSampleBufferRef sampleBuffer = nullptr;
    if (auto error = PAL::CMSampleBufferCreate(kCFAllocatorDefault, blockBuffer.get(), true, nullptr, nullptr, videoFormat, 1, 0, nullptr, 0, nullptr, &sampleBuffer)) {
        RELEASE_LOG_ERROR(WebRTC, "AV1BufferToCMSampleBuffer CMSampleBufferCreate failed with: %d", error);
        return nullptr;
    }

    return adoptCF(sampleBuffer);
}

static void av1DecompressionOutputCallback(void* decoderRef, void* params, OSStatus status, VTDecodeInfoFlags, CVImageBufferRef imageBuffer, CMTime timestamp, CMTime)
{
    std::unique_ptr<RTCFrameDecodeParams> decodeParams(static_cast<RTCFrameDecodeParams*>(params));
    if (status != noErr || !imageBuffer) {
        RTCVideoDecoderVTBAV1 *decoder = (__bridge RTCVideoDecoderVTBAV1 *)decoderRef;
        [decoder setError:status != noErr ? status : 1];
        RELEASE_LOG_ERROR(WebRTC, "RTCVideoDecoderVTBAV1 failed to decode with status: %d", status);
        decodeParams->callback.get()(nil, 0, 0, false);
        return;
    }

    static const int64_t kNumNanosecsPerSec = 1000000000;
    decodeParams->callback.get()(imageBuffer, decodeParams->timestamp, PAL::CMTimeGetSeconds(timestamp) * kNumNanosecsPerSec, false);
}

@implementation RTCVideoDecoderVTBAV1 {
    RetainPtr<CMVideoFormatDescriptionRef> _videoFormat;
    RetainPtr<VTDecompressionSessionRef> _decompressionSession;
    BlockPtr<void(CVPixelBufferRef, long long, long long, bool)> _callback;
    OSStatus _error;
    int32_t _width;
    int32_t _height;
    bool _shouldCheckFormat;
}

- (instancetype)init {
    self = [super init];
    if (self)
        _shouldCheckFormat = true;

    return self;
}

- (void)dealloc {
    [self destroyDecompressionSession];
    _videoFormat = nullptr;
    [super dealloc];
}

- (void)setWidth:(uint16_t)width height:(uint16_t)height {
    _width = width;
    _height = height;
    _shouldCheckFormat = true;
}

- (NSInteger)decodeData:(const uint8_t *)data size:(size_t)size timeStamp:(int64_t)timeStamp {
    if (_error != noErr) {
        RELEASE_LOG_ERROR(WebRTC, "RTCVideoDecoderVTBAV1 Last frame decode failed");
        _error = noErr;
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    if (_shouldCheckFormat || !_videoFormat) {
        auto inputFormat = computeAV1InputFormat({ data, size }, _width, _height);
        if (inputFormat) {
            _shouldCheckFormat = false;
            if (!PAL::CMFormatDescriptionEqual(inputFormat.get(), _videoFormat.get())) {
                _videoFormat = WTFMove(inputFormat);
                int resetDecompressionSessionError = [self resetDecompressionSession];
                if (resetDecompressionSessionError != WEBRTC_VIDEO_CODEC_OK) {
                    _videoFormat = nullptr;
                    return resetDecompressionSessionError;
                }
            }
        }
    }

    if (!_videoFormat)
        return WEBRTC_VIDEO_CODEC_ERROR;

    auto sampleBuffer = av1BufferToCMSampleBuffer({ data, size }, _videoFormat.get());
    if (!sampleBuffer)
        return WEBRTC_VIDEO_CODEC_ERROR;

    VTDecodeFrameFlags decodeFlags = kVTDecodeFrame_EnableAsynchronousDecompression;
    std::unique_ptr<RTCFrameDecodeParams> frameDecodeParams(new RTCFrameDecodeParams { _callback, timeStamp });
    auto status = VTDecompressionSessionDecodeFrame(_decompressionSession.get(), sampleBuffer.get(), decodeFlags, frameDecodeParams.release(), nullptr);

#if PLATFORM(IOS_FAMILY)
    if ((status == kVTInvalidSessionErr || status == kVTVideoDecoderMalfunctionErr) && [self resetDecompressionSession] == WEBRTC_VIDEO_CODEC_OK) {
        RELEASE_LOG_INFO(WebRTC, "Failed to decode frame with code %d, retrying decode after decompression session reset", status);
        frameDecodeParams.reset(new RTCFrameDecodeParams { _callback, timeStamp });
        status = VTDecompressionSessionDecodeFrame(_decompressionSession.get(), sampleBuffer.get(), decodeFlags, frameDecodeParams.release(), nullptr);
    }
#endif

    if (status != noErr) {
        RELEASE_LOG_ERROR(WebRTC, "RTCVideoDecoderVTBAV1 failed to decode frame with code %d", status);
        return WEBRTC_VIDEO_CODEC_ERROR;
    }
    return WEBRTC_VIDEO_CODEC_OK;
}

- (void)setCallback:(RTCVideoDecoderVTBAV1Callback)callback {
    _callback = callback;
}

- (void)setError:(OSStatus)error {
    _error = error;
}

- (NSInteger)releaseDecoder {
    [self destroyDecompressionSession];
    _videoFormat = nullptr;
    _callback = nullptr;
    return WEBRTC_VIDEO_CODEC_OK;
}

#pragma mark - Private

- (int)resetDecompressionSession {
    [self destroyDecompressionSession];

    if (!_videoFormat)
        return WEBRTC_VIDEO_CODEC_OK;

    static size_t const attributesSize = 3;
    CFTypeRef keys[attributesSize] = {
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
        WebCore::kCVPixelBufferOpenGLCompatibilityKey,
#elif PLATFORM(IOS_FAMILY)
        WebCore::get_CoreVideo_kCVPixelBufferOpenGLESCompatibilityKey(),
#endif
        WebCore::kCVPixelBufferIOSurfacePropertiesKey,
        WebCore::kCVPixelBufferPixelFormatTypeKey
    };

    auto ioSurfaceValue = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, nullptr, nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    int64_t nv12type = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
    auto pixelFormat = adoptCF(CFNumberCreate(nullptr, kCFNumberLongType, &nv12type));
    CFTypeRef values[attributesSize] = { kCFBooleanTrue, ioSurfaceValue.get(), pixelFormat.get() };
    auto attributes = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, attributesSize, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    VTDecompressionOutputCallbackRecord record { av1DecompressionOutputCallback, (__bridge void *)self };
    VTDecompressionSessionRef decompressionSession;
    auto status = VTDecompressionSessionCreate(nullptr, _videoFormat.get(), nullptr, attributes.get(), &record, &decompressionSession);

    if (status != noErr) {
        RELEASE_LOG_ERROR(WebRTC, "RTCVideoDecoderVTBAV1 failed to create decompression session %d", status);
        [self destroyDecompressionSession];
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    _decompressionSession = adoptCF(decompressionSession);
    [self configureDecompressionSession];
    return WEBRTC_VIDEO_CODEC_OK;
}

- (void)configureDecompressionSession {
    ASSERT(_decompressionSession);
#if PLATFORM(IOS_FAMILY)
    VTSessionSetProperty(_decompressionSession.get(), kVTDecompressionPropertyKey_RealTime, kCFBooleanTrue);
#endif
}

- (void)destroyDecompressionSession {
    if (_decompressionSession) {
        VTDecompressionSessionWaitForAsynchronousFrames(_decompressionSession.get());
        VTDecompressionSessionInvalidate(_decompressionSession.get());
        _decompressionSession = nullptr;
    }
}

- (void)flush {
    if (_decompressionSession)
        VTDecompressionSessionWaitForAsynchronousFrames(_decompressionSession.get());
}

@end

#endif // USE(LIBWEBRTC)
