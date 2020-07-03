/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 */

#import "config.h"
#import "ImageDecoderAVFObjC.h"

#if HAVE(AVASSETREADER)

#import "AVAssetMIMETypeCache.h"
#import "AffineTransform.h"
#import "ContentType.h"
#import "FloatQuad.h"
#import "FloatRect.h"
#import "FloatSize.h"
#import "ImageRotationSessionVT.h"
#import "Logging.h"
#import "MIMETypeRegistry.h"
#import "MediaSampleAVFObjC.h"
#import "SharedBuffer.h"
#import "UTIUtilities.h"
#import "WebCoreDecompressionSession.h"
#import <AVFoundation/AVAsset.h>
#import <AVFoundation/AVAssetReader.h>
#import <AVFoundation/AVAssetReaderOutput.h>
#import <AVFoundation/AVAssetResourceLoader.h>
#import <AVFoundation/AVAssetTrack.h>
#import <AVFoundation/AVTime.h>
#import <VideoToolbox/VTUtilities.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <wtf/MainThread.h>
#import <wtf/MediaTime.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/Optional.h>
#import <wtf/Vector.h>

#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

#pragma mark -

@interface WebCoreSharedBufferResourceLoaderDelegate : NSObject<AVAssetResourceLoaderDelegate> {
    WebCore::ImageDecoderAVFObjC* _parent;
    long long _expectedContentSize;
    RetainPtr<NSData> _data;
    bool _complete;
    Vector<RetainPtr<AVAssetResourceLoadingRequest>> _requests;
    Lock _dataLock;
}
@property (readonly) NSData* data;
- (id)initWithParent:(WebCore::ImageDecoderAVFObjC*)parent;
- (void)setExpectedContentSize:(long long)expectedContentSize;
- (void)updateData:(NSData *)data complete:(BOOL)complete;
- (BOOL)canFulfillRequest:(AVAssetResourceLoadingRequest *)loadingRequest;
- (void)enqueueRequest:(AVAssetResourceLoadingRequest *)loadingRequest;
- (void)fulfillPendingRequests;
- (void)fulfillRequest:(AVAssetResourceLoadingRequest *)loadingRequest;
@end

@implementation WebCoreSharedBufferResourceLoaderDelegate
- (id)initWithParent:(WebCore::ImageDecoderAVFObjC*)parent
{
    if (!(self = [super init]))
        return nil;
    _parent = parent;

    return self;
}

- (NSData*)data
{
    return _data.get();
}

- (void)setExpectedContentSize:(long long)expectedContentSize
{
    LockHolder holder { _dataLock };
    _expectedContentSize = expectedContentSize;

    [self fulfillPendingRequests];
}

- (void)updateData:(NSData *)data complete:(BOOL)complete
{
    LockHolder holder { _dataLock };
    _data = data;
    _complete = complete;

    [self fulfillPendingRequests];
}

- (BOOL)canFulfillRequest:(AVAssetResourceLoadingRequest *)request
{
    if (!request)
        return NO;

    if (request.finished || request.cancelled)
        return NO;

    // AVURLAsset's resource loader requires knowing the expected content size
    // to load sucessfully. That requires either having the complete data for
    // the resource, or knowing the expected content size. 
    if (!_complete && !_expectedContentSize)
        return NO;

    if (auto dataRequest = request.dataRequest) {
        if (dataRequest.requestedOffset > static_cast<long long>(_data.get().length))
            return NO;
    }

    return YES;
}

- (void)enqueueRequest:(AVAssetResourceLoadingRequest *)loadingRequest
{
    ASSERT(!_requests.contains(loadingRequest));
    _requests.append(loadingRequest);
}

- (void)fulfillPendingRequests
{
    for (auto& request : _requests) {
        if ([self canFulfillRequest:request.get()])
            [self fulfillRequest:request.get()];
    }

    _requests.removeAllMatching([] (auto& request) {
        return request.get().finished;
    });
}

- (void)fulfillRequest:(AVAssetResourceLoadingRequest *)request
{
    if (auto infoRequest = request.contentInformationRequest) {
        infoRequest.contentType = _parent->uti();
        infoRequest.byteRangeAccessSupported = YES;
        infoRequest.contentLength = _complete ? _data.get().length : _expectedContentSize;
    }

    if (auto dataRequest = request.dataRequest) {
        long long availableLength = _data.get().length - dataRequest.requestedOffset;
        if (availableLength <= 0)
            return;

        long long requestedLength;
        if (dataRequest.requestsAllDataToEndOfResource)
            requestedLength = availableLength;
        else
            requestedLength = std::min<long long>(availableLength, dataRequest.requestedLength);

        auto range = NSMakeRange(static_cast<NSUInteger>(dataRequest.requestedOffset), static_cast<NSUInteger>(requestedLength));
        NSData* requestedData = [_data subdataWithRange:range];
        if (!requestedData)
            return;

        [dataRequest respondWithData:requestedData];

        if (dataRequest.requestsAllDataToEndOfResource) {
            if (!_complete)
                return;
        } else if (dataRequest.requestedOffset + dataRequest.requestedLength > dataRequest.currentOffset)
            return;
    }

    [request finishLoading];
}

- (BOOL)resourceLoader:(AVAssetResourceLoader *)resourceLoader shouldWaitForLoadingOfRequestedResource:(AVAssetResourceLoadingRequest *)loadingRequest
{
    LockHolder holder { _dataLock };

    UNUSED_PARAM(resourceLoader);

    if ([self canFulfillRequest:loadingRequest]) {
        [self fulfillRequest:loadingRequest];
        if (loadingRequest.finished)
            return YES;
    }

    [self enqueueRequest:loadingRequest];
    return YES;
}

- (void)resourceLoader:(AVAssetResourceLoader *)resourceLoader didCancelLoadingRequest:(AVAssetResourceLoadingRequest *)loadingRequest
{
    LockHolder holder { _dataLock };

    UNUSED_PARAM(resourceLoader);
    _requests.removeAll(loadingRequest);
}
@end

namespace WebCore {

#pragma mark - Static Methods

static NSURL *customSchemeURL()
{
    static NSURL *url = [[NSURL alloc] initWithString:@"custom-imagedecoderavfobjc://resource"];
    return url;
}

static NSDictionary *imageDecoderAssetOptions()
{
    static NSDictionary *options = [] {
        return [@{
            AVURLAssetReferenceRestrictionsKey: @(AVAssetReferenceRestrictionForbidAll),
            AVURLAssetUsesNoPersistentCacheKey: @YES,
        } retain];
    }();
    return options;
}

class ImageDecoderAVFObjCSample : public MediaSampleAVFObjC {
public:
    static Ref<ImageDecoderAVFObjCSample> create(RetainPtr<CMSampleBufferRef>&& sampleBuffer)
    {
        return adoptRef(*new ImageDecoderAVFObjCSample(WTFMove(sampleBuffer)));
    }

    CGImageRef image() const { return m_image.get(); }
    void setImage(RetainPtr<CGImageRef>&& image)
    {
        m_image = WTFMove(image);
        if (!m_image) {
            m_hasAlpha = false;
            return;
        }

        auto alphaInfo = CGImageGetAlphaInfo(m_image.get());
        m_hasAlpha = alphaInfo != kCGImageAlphaNone && alphaInfo != kCGImageAlphaNoneSkipLast && alphaInfo != kCGImageAlphaNoneSkipFirst;
    }

    struct ByteRange {
        size_t byteOffset { 0 };
        size_t byteLength { 0 };
    };

    Optional<ByteRange> byteRange() const
    {
        if (PAL::CMSampleBufferGetDataBuffer(m_sample.get())
            || PAL::CMSampleBufferGetImageBuffer(m_sample.get())
            || !PAL::CMSampleBufferDataIsReady(m_sample.get()))
            return WTF::nullopt;

        CFNumberRef byteOffsetCF = (CFNumberRef)PAL::CMGetAttachment(m_sample.get(), PAL::kCMSampleBufferAttachmentKey_SampleReferenceByteOffset, nullptr);
        if (!byteOffsetCF || CFGetTypeID(byteOffsetCF) != CFNumberGetTypeID())
            return WTF::nullopt;

        int64_t byteOffset { 0 };
        if (!CFNumberGetValue(byteOffsetCF, kCFNumberSInt64Type, &byteOffset))
            return WTF::nullopt;

        CMItemCount sizeArrayEntries = 0;
        PAL::CMSampleBufferGetSampleSizeArray(m_sample.get(), 0, nullptr, &sizeArrayEntries);
        if (sizeArrayEntries != 1)
            return WTF::nullopt;

        size_t singleSizeEntry;
        PAL::CMSampleBufferGetSampleSizeArray(m_sample.get(), 1, &singleSizeEntry, nullptr);
        return {{static_cast<size_t>(byteOffset), singleSizeEntry}};
    }

    SampleFlags flags() const override
    {
        return (SampleFlags)(MediaSampleAVFObjC::flags() | (m_hasAlpha ? HasAlpha : 0));
    }

private:
    ImageDecoderAVFObjCSample(RetainPtr<CMSampleBufferRef>&& sample)
        : MediaSampleAVFObjC(WTFMove(sample))
    {
    }

    RetainPtr<CGImageRef> m_image;
    bool m_hasAlpha { false };
};

static ImageDecoderAVFObjCSample* toSample(const PresentationOrderSampleMap::value_type& pair)
{
    return (ImageDecoderAVFObjCSample*)pair.second.get();
}

template <typename Iterator>
ImageDecoderAVFObjCSample* toSample(Iterator iter)
{
    return (ImageDecoderAVFObjCSample*)iter->second.get();
}

#pragma mark - ImageDecoderAVFObjC

RefPtr<ImageDecoderAVFObjC> ImageDecoderAVFObjC::create(SharedBuffer& data, const String& mimeType, AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
{
    // AVFoundation may not be available at runtime.
    if (!AVAssetMIMETypeCache::singleton().isAvailable())
        return nullptr;

    if (!canLoad_VideoToolbox_VTCreateCGImageFromCVPixelBuffer())
        return nullptr;

    return adoptRef(*new ImageDecoderAVFObjC(data, mimeType, alphaOption, gammaAndColorProfileOption));
}

ImageDecoderAVFObjC::ImageDecoderAVFObjC(SharedBuffer& data, const String& mimeType, AlphaOption, GammaAndColorProfileOption)
    : ImageDecoder()
    , m_mimeType(mimeType)
    , m_uti(WebCore::UTIFromMIMEType(mimeType))
    , m_asset(adoptNS([PAL::allocAVURLAssetInstance() initWithURL:customSchemeURL() options:imageDecoderAssetOptions()]))
    , m_loader(adoptNS([[WebCoreSharedBufferResourceLoaderDelegate alloc] initWithParent:this]))
    , m_decompressionSession(WebCoreDecompressionSession::createRGB())
{
    [m_loader updateData:data.createNSData().get() complete:NO];

    [m_asset.get().resourceLoader setDelegate:m_loader.get() queue:dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0)];
    [m_asset loadValuesAsynchronouslyForKeys:@[@"tracks"] completionHandler:[protectedThis = makeRefPtr(this)] () mutable {
        callOnMainThread([protectedThis = WTFMove(protectedThis)] {
            protectedThis->setTrack(protectedThis->firstEnabledTrack());
        });
    }];
}

ImageDecoderAVFObjC::~ImageDecoderAVFObjC() = default;

bool ImageDecoderAVFObjC::supportsMediaType(MediaType type)
{
    return type == MediaType::Video && AVAssetMIMETypeCache::singleton().isAvailable();
}

bool ImageDecoderAVFObjC::supportsContainerType(const String& type)
{
    return AVAssetMIMETypeCache::singleton().supportsContainerType(type);
}

bool ImageDecoderAVFObjC::canDecodeType(const String& mimeType)
{
    return AVAssetMIMETypeCache::singleton().canDecodeType(mimeType) != MediaPlayerEnums::SupportsType::IsNotSupported;
}

AVAssetTrack *ImageDecoderAVFObjC::firstEnabledTrack()
{
    NSArray<AVAssetTrack *> *videoTracks = [m_asset tracksWithMediaCharacteristic:AVMediaCharacteristicVisual];
    NSUInteger firstEnabledIndex = [videoTracks indexOfObjectPassingTest:^(AVAssetTrack *track, NSUInteger, BOOL*) {
        return track.enabled;
    }];

    if (firstEnabledIndex == NSNotFound) {
        LOG(Images, "ImageDecoderAVFObjC::firstEnabledTrack(%p) - asset has no enabled video tracks", this);
        return nil;
    }

    return [videoTracks objectAtIndex:firstEnabledIndex];
}

void ImageDecoderAVFObjC::readSamples()
{
    if (!m_sampleData.empty())
        return;

    auto assetReader = adoptNS([PAL::allocAVAssetReaderInstance() initWithAsset:m_asset.get() error:nil]);
    auto referenceOutput = adoptNS([PAL::allocAVAssetReaderSampleReferenceOutputInstance() initWithTrack:m_track.get()]);

    referenceOutput.get().alwaysCopiesSampleData = NO;
    [assetReader addOutput:referenceOutput.get()];
    [assetReader startReading];

    while (auto sampleBuffer = adoptCF([referenceOutput copyNextSampleBuffer])) {
        // NOTE: Some samples emitted by the AVAssetReader simply denote the boundary of edits
        // and do not carry media data.
        if (!(PAL::CMSampleBufferGetNumSamples(sampleBuffer.get())))
            continue;
        m_sampleData.addSample(ImageDecoderAVFObjCSample::create(WTFMove(sampleBuffer)).get());
    }

    if (m_encodedDataStatusChangedCallback)
        m_encodedDataStatusChangedCallback(encodedDataStatus());
}

void ImageDecoderAVFObjC::readTrackMetadata()
{
    AffineTransform finalTransform = CGAffineTransformConcat(m_asset.get().preferredTransform, m_track.get().preferredTransform);
    auto size = expandedIntSize(FloatSize(m_track.get().naturalSize));
    if (finalTransform.isIdentity()) {
        m_size = size;
        m_imageRotationSession = nullptr;
        return;
    }

    if (!m_imageRotationSession
        || !m_imageRotationSession->transform()
        || m_imageRotationSession->transform().value() != finalTransform
        || m_imageRotationSession->size() != size)
        m_imageRotationSession = makeUnique<ImageRotationSessionVT>(WTFMove(finalTransform), size, kCVPixelFormatType_32BGRA, ImageRotationSessionVT::IsCGImageCompatible::Yes);

    m_size = expandedIntSize(m_imageRotationSession->rotatedSize());
}

bool ImageDecoderAVFObjC::storeSampleBuffer(CMSampleBufferRef sampleBuffer)
{
    auto pixelBuffer = m_decompressionSession->decodeSampleSync(sampleBuffer);
    if (!pixelBuffer) {
        RELEASE_LOG_ERROR(Images, "ImageDecoderAVFObjC::storeSampleBuffer(%p) - could not decode sampleBuffer", this);
        return false;
    }

    auto presentationTime = PAL::toMediaTime(PAL::CMSampleBufferGetPresentationTimeStamp(sampleBuffer));
    auto iter = m_sampleData.presentationOrder().findSampleWithPresentationTime(presentationTime);

    if (m_imageRotationSession)
        pixelBuffer = m_imageRotationSession->rotate(pixelBuffer.get());

    CGImageRef rawImage = nullptr;
    if (noErr != VTCreateCGImageFromCVPixelBuffer(pixelBuffer.get(), nullptr, &rawImage)) {
        RELEASE_LOG_ERROR(Images, "ImageDecoderAVFObjC::storeSampleBuffer(%p) - could not create CGImage from pixelBuffer", this);
        return false;
    }

    if (iter == m_sampleData.presentationOrder().end()) {
        RELEASE_LOG_ERROR(Images, "ImageDecoderAVFObjC::storeSampleBuffer(%p) - could not find sample buffer entry with specified presentation time", this);
        return false;
    }

    toSample(iter)->setImage(adoptCF(rawImage));

    return true;
}

void ImageDecoderAVFObjC::advanceCursor()
{
    if (m_cursor == m_sampleData.decodeOrder().end() || ++m_cursor == m_sampleData.decodeOrder().end())
        m_cursor = m_sampleData.decodeOrder().begin();
}

void ImageDecoderAVFObjC::setTrack(AVAssetTrack *track)
{
    if (m_track == track)
        return;
    m_track = track;

    LockHolder holder { m_sampleGeneratorLock };
    m_sampleData.clear();
    m_size.reset();
    m_cursor = m_sampleData.decodeOrder().end();
    m_imageRotationSession = nullptr;

    [track loadValuesAsynchronouslyForKeys:@[@"naturalSize", @"preferredTransform"] completionHandler:[protectedThis = makeRefPtr(this)] () mutable {
        callOnMainThread([protectedThis = WTFMove(protectedThis)] {
            protectedThis->readTrackMetadata();
            protectedThis->readSamples();
        });
    }];
}

void ImageDecoderAVFObjC::setEncodedDataStatusChangeCallback(WTF::Function<void(EncodedDataStatus)>&& callback)
{
    m_encodedDataStatusChangedCallback = WTFMove(callback);
}

EncodedDataStatus ImageDecoderAVFObjC::encodedDataStatus() const
{
    if (!m_sampleData.empty())
        return EncodedDataStatus::Complete;
    if (m_size)
        return EncodedDataStatus::SizeAvailable;
    if (m_track)
        return EncodedDataStatus::TypeAvailable;
    return EncodedDataStatus::Unknown;
}

IntSize ImageDecoderAVFObjC::size() const
{
    if (m_size)
        return m_size.value();
    return IntSize();
}

size_t ImageDecoderAVFObjC::frameCount() const
{
    return m_sampleData.size();
}

RepetitionCount ImageDecoderAVFObjC::repetitionCount() const
{
    // In the absence of instructions to the contrary, assume all media formats repeat infinitely.
    // FIXME: Future media formats may embed repeat count information, and when that is available
    // through AVAsset, account for it here.
    return frameCount() > 1 ? RepetitionCountInfinite : RepetitionCountNone;
}

String ImageDecoderAVFObjC::uti() const
{
    return m_uti;
}

String ImageDecoderAVFObjC::filenameExtension() const
{
    return MIMETypeRegistry::preferredExtensionForMIMEType(m_mimeType);
}

IntSize ImageDecoderAVFObjC::frameSizeAtIndex(size_t, SubsamplingLevel) const
{
    return size();
}

bool ImageDecoderAVFObjC::frameIsCompleteAtIndex(size_t index) const
{
    auto* sampleData = sampleAtIndex(index);
    if (!sampleData)
        return false;

    return sampleIsComplete(*sampleData);
}

ImageOrientation ImageDecoderAVFObjC::frameOrientationAtIndex(size_t) const
{
    return ImageOrientation::None;
}

Seconds ImageDecoderAVFObjC::frameDurationAtIndex(size_t index) const
{
    auto* sampleData = sampleAtIndex(index);
    if (!sampleData)
        return { };

    return Seconds(sampleData->duration().toDouble());
}

bool ImageDecoderAVFObjC::frameHasAlphaAtIndex(size_t index) const
{
    auto* sampleData = sampleAtIndex(index);
    return sampleData ? sampleData->hasAlpha() : false;
}

bool ImageDecoderAVFObjC::frameAllowSubsamplingAtIndex(size_t index) const
{
    return index <= m_sampleData.size();
}

unsigned ImageDecoderAVFObjC::frameBytesAtIndex(size_t index, SubsamplingLevel subsamplingLevel) const
{
    if (!frameIsCompleteAtIndex(index))
        return 0;

    IntSize frameSize = frameSizeAtIndex(index, subsamplingLevel);
    return (frameSize.area() * 4).unsafeGet();
}

NativeImagePtr ImageDecoderAVFObjC::createFrameImageAtIndex(size_t index, SubsamplingLevel, const DecodingOptions&)
{
    LockHolder holder { m_sampleGeneratorLock };

    auto* sampleData = sampleAtIndex(index);
    if (!sampleData)
        return nullptr;

    if (auto image = sampleData->image())
        return image;

    if (m_cursor == m_sampleData.decodeOrder().end())
        m_cursor = m_sampleData.decodeOrder().begin();

    auto decodeTime = sampleData->decodeTime();

    if (decodeTime < m_cursor->second->decodeTime()) {
        // Rewind cursor to the last sync sample to begin decoding
        m_cursor = m_sampleData.decodeOrder().findSampleWithDecodeKey({decodeTime, sampleData->presentationTime()});
        do {
            if (m_cursor->second->isSync())
                break;
        } while (--m_cursor != m_sampleData.decodeOrder().begin());
    }

    RetainPtr<CGImageRef> image;
    while (true) {
        if (decodeTime < m_cursor->second->decodeTime())
            return nullptr;

        auto cursorSample = toSample(m_cursor);
        if (!cursorSample)
            return nullptr;

        if (!sampleIsComplete(*cursorSample))
            return nullptr;

        if (auto byteRange = cursorSample->byteRange()) {
            auto& byteRangeValue = byteRange.value();
            auto* data = m_loader.get().data;
            CMBlockBufferCustomBlockSource source {
                0,
                nullptr,
                [](void* refcon, void*, size_t) {
                    [(id)refcon release];
                },
                [data retain]
            };
            CMBlockBufferRef rawBlockBuffer = nullptr;
            if (noErr != PAL::CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, const_cast<void*>(data.bytes), data.length, nullptr, &source, byteRangeValue.byteOffset, byteRangeValue.byteLength, 0, &rawBlockBuffer))
                return nullptr;

            if (!rawBlockBuffer)
                return nullptr;

            if (noErr != PAL::CMSampleBufferSetDataBuffer(cursorSample->sampleBuffer(), rawBlockBuffer))
                return nullptr;
            CFRelease(rawBlockBuffer);

            PAL::CMRemoveAttachment(cursorSample->sampleBuffer(), PAL::kCMSampleBufferAttachmentKey_SampleReferenceByteOffset);
            PAL::CMRemoveAttachment(cursorSample->sampleBuffer(), PAL::kCMSampleBufferAttachmentKey_SampleReferenceURL);
        }

        auto cursorSampleBuffer = cursorSample->sampleBuffer();
        if (!cursorSampleBuffer)
            break;

        if (!storeSampleBuffer(cursorSampleBuffer))
            break;

        advanceCursor();
        if (auto image = sampleData->image())
            return image;
    }

    advanceCursor();
    return nullptr;
}

void ImageDecoderAVFObjC::setExpectedContentSize(long long expectedContentSize)
{
    m_loader.get().expectedContentSize = expectedContentSize;
}

void ImageDecoderAVFObjC::setData(SharedBuffer& data, bool allDataReceived)
{
    [m_loader updateData:data.createNSData().get() complete:allDataReceived];

    if (allDataReceived) {
        m_isAllDataReceived = true;

        if (!m_track)
            setTrack(firstEnabledTrack());

        if (!m_track)
            return;

        readTrackMetadata();
        readSamples();
    }
}

void ImageDecoderAVFObjC::clearFrameBufferCache(size_t index)
{
    size_t i = 0;
    for (auto& samplePair : m_sampleData.presentationOrder()) {
        toSample(samplePair)->setImage(nullptr);
        if (++i > index)
            break;
    }
}

const ImageDecoderAVFObjCSample* ImageDecoderAVFObjC::sampleAtIndex(size_t index) const
{
    if (index >= m_sampleData.presentationOrder().size())
        return nullptr;

    // FIXME: std::map is not random-accessible; this can get expensive if callers repeatedly call
    // with monotonically increasing indexes. Investigate adding an O(1) side structure to make this
    // style of access faster.
    auto iter = m_sampleData.presentationOrder().begin();
    for (size_t i = 0; i != index; ++i)
        ++iter;

    return toSample(iter);
}

bool ImageDecoderAVFObjC::sampleIsComplete(const ImageDecoderAVFObjCSample& sample) const
{
    if (auto byteRange = sample.byteRange()) {
        auto& byteRangeValue = byteRange.value();
        return byteRangeValue.byteOffset + byteRangeValue.byteLength <= m_loader.get().data.length;
    }

    return PAL::CMSampleBufferDataIsReady(sample.sampleBuffer());
}

}

#endif
