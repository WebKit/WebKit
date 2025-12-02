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
#import "WebCoreDecompressionSession.h"

#import "FormatDescriptionUtilities.h"
#import "IOSurface.h"
#import "Logging.h"
#import "MediaSampleAVFObjC.h"
#import "PixelBufferConformerCV.h"
#import "VideoDecoder.h"
#import "VideoFrame.h"
#import <CoreFoundation/CoreFoundation.h>
#import <CoreMedia/CMBufferQueue.h>
#import <CoreMedia/CMFormatDescription.h>
#import <map>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <wtf/BlockPtr.h>
#import <wtf/MainThread.h>
#import <wtf/MediaTime.h>
#import <wtf/NativePromise.h>
#import <wtf/RunLoop.h>
#import <wtf/StringPrintStream.h>
#import <wtf/Vector.h>
#import <wtf/WTFSemaphore.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/cf/VectorCF.h>

#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

static bool canCopyFormatDescriptionExtension()
{
    static bool canCopyFormatDescriptionExtension = false;
#if PLATFORM(VISION)
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        canCopyFormatDescriptionExtension = PAL::canLoad_CoreMedia_kCMFormatDescriptionExtension_ProjectionKind()
            && PAL::canLoad_CoreMedia_kCMFormatDescriptionExtension_ViewPackingKind()
            && PAL::canLoad_CoreMedia_kCMFormatDescriptionExtension_CameraCalibrationDataLensCollection();
    });
#endif
    return canCopyFormatDescriptionExtension;
}

Ref<WebCoreDecompressionSession> WebCoreDecompressionSession::createOpenGL(GuaranteedSerialFunctionDispatcher* dispatcher)
{
    return adoptRef(*new WebCoreDecompressionSession(nullptr, dispatcher));
}

Ref<WebCoreDecompressionSession> WebCoreDecompressionSession::createRGB(GuaranteedSerialFunctionDispatcher* dispatcher)
{
    return adoptRef(*new WebCoreDecompressionSession(@{
        (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA),
        (__bridge NSString *)kCVPixelBufferIOSurfacePropertiesKey: @{ /* empty dictionary */ }
    }, dispatcher));
}

NSDictionary *WebCoreDecompressionSession::defaultPixelBufferAttributes()
{
    return @{
        (__bridge NSString *)kCVPixelBufferIOSurfaceCoreAnimationCompatibilityKey: (id)kCFBooleanTrue,
        (__bridge NSString *)kCVPixelBufferPreferRealTimeCacheModeIfEveryoneDoesKey: (id)kCFBooleanTrue,
        (__bridge NSString *)kCVPixelBufferIOSurfacePropertiesKey: @{ /* empty dictionary */ }
    };
}

WebCoreDecompressionSession::WebCoreDecompressionSession(NSDictionary *pixelBufferAttributes, GuaranteedSerialFunctionDispatcher* dispatcher)
    : m_pixelBufferAttributes(pixelBufferAttributes ? pixelBufferAttributes : defaultPixelBufferAttributes())
    , m_dispatcher(dispatcher ? *dispatcher : queueSingleton())
    , m_stereoSupported(VTIsStereoMVHEVCDecodeSupported())
{
}

WebCoreDecompressionSession::~WebCoreDecompressionSession() = default;

WorkQueue& WebCoreDecompressionSession::queueSingleton()
{
    static std::once_flag onceKey;
    static LazyNeverDestroyed<Ref<WorkQueue>> workQueue;
    std::call_once(onceKey, [] {
        workQueue.construct(WorkQueue::create("WebCoreDecompressionSession Decompression Queue"_s));
    });
    return workQueue.get();
}

void WebCoreDecompressionSession::invalidate()
{
    assertIsMainThread();
    m_invalidated = true;
    Locker lock { m_lock };
    m_dispatcher->dispatch([decoder = WTFMove(m_videoDecoder)] {
        if (decoder)
            decoder->close();
    });
}

void WebCoreDecompressionSession::assignResourceOwner(CVImageBufferRef imageBuffer)
{
    if (!m_resourceOwner || !imageBuffer || CFGetTypeID(imageBuffer) != CVPixelBufferGetTypeID())
        return;
    if (RetainPtr surface = CVPixelBufferGetIOSurface((CVPixelBufferRef)imageBuffer))
        IOSurface::setOwnershipIdentity(surface.get(), m_resourceOwner);
}

static RetainPtr<CFArrayRef> createMVHEVCVideoLayersArray(CMVideoFormatDescriptionRef videoFormatDescription)
{
    if (!videoFormatDescription)
        return { };

    // Get the tag collection array
    CFArrayRef rawTagCollectionArray = nullptr;
    if (auto status = PAL::CMVideoFormatDescriptionCopyTagCollectionArray(videoFormatDescription, &rawTagCollectionArray); status != noErr)
        return { };

    RetainPtr tagCollectionArray = adoptCF(rawTagCollectionArray);
    // Get the layer IDs to decode
    if (CFArrayGetCount(tagCollectionArray.get()) <= 1)
        return { };

    RetainPtr layersArray = adoptCF(CFArrayCreateMutable(kCFAllocatorDefault, 1, &kCFTypeArrayCallBacks));
    if (!layersArray)
        return { };

    for (CFIndex i = 0; i < CFArrayGetCount(tagCollectionArray.get()); ++i) {
        RetainPtr tagCollection = (CMTagCollectionRef)CFArrayGetValueAtIndex(tagCollectionArray.get(), i);
        CMTag videoLayerIDTag = PAL::kCMTagInvalid;
        CMItemCount numberOfTagsCopied = 0;
        if (auto status = PAL::CMTagCollectionGetTagsWithCategory(tagCollection.get(), kCMTagCategory_VideoLayerID, &videoLayerIDTag, 1, &numberOfTagsCopied); status == noErr && numberOfTagsCopied == 1) {
            auto videoLayerID = PAL::CMTagGetSInt64Value(videoLayerIDTag);
            if (RetainPtr number = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &videoLayerID)))
                CFArrayAppendValue(layersArray.get(), number.get());
        }
    }
    return layersArray;
}

static std::map<int64_t, uint64_t> createMVHEVCStereoViewMap(CMVideoFormatDescriptionRef videoFormatDescription)
{
    // Get the tag collection array
    CFArrayRef rawTagCollectionArray = nullptr;
    if (auto status = PAL::CMVideoFormatDescriptionCopyTagCollectionArray(videoFormatDescription, &rawTagCollectionArray); status != noErr)
        return { };

    RetainPtr tagCollectionArray = adoptCF(rawTagCollectionArray);
    // Get the layer IDs to decode
    if (CFArrayGetCount(tagCollectionArray.get()) <= 1)
        return { };

    std::map<int64_t, uint64_t> stereoViewMap;

    for (CFIndex i = 0; i < CFArrayGetCount(tagCollectionArray.get()); ++i) {
        RetainPtr tagCollection = (CMTagCollectionRef)CFArrayGetValueAtIndex(tagCollectionArray.get(), i);
        CMTag videoLayerIDTag = PAL::kCMTagInvalid;
        CMItemCount numberOfTagsCopied = 0;
        if (auto status = PAL::CMTagCollectionGetTagsWithCategory(tagCollection.get(), kCMTagCategory_VideoLayerID, &videoLayerIDTag, 1, &numberOfTagsCopied); status == noErr && numberOfTagsCopied == 1) {
            auto videoLayerID = PAL::CMTagGetSInt64Value(videoLayerIDTag);
            CMTag stereoViewTag = PAL::kCMTagInvalid;
            if (auto status = PAL::CMTagCollectionGetTagsWithCategory(tagCollection.get(), kCMTagCategory_StereoView, &stereoViewTag, 1, &numberOfTagsCopied); status == noErr && numberOfTagsCopied == 1) {
                auto stereoView = PAL::CMTagGetFlagsValue(stereoViewTag);
                stereoViewMap[videoLayerID] = stereoView;
            }
        }
    }
    return stereoViewMap;
}

static RetainPtr<CFArrayRef> createTagCollectionRequiredFromVideoToolboxOutput(CMTaggedBufferGroupRef bufferGroup, const std::map<int64_t, uint64_t>& layerIDToStereoViewTable)
{
    ASSERT(bufferGroup);

    CFIndex bufferGroupCount = PAL::CMTaggedBufferGroupGetCount(bufferGroup);
    RetainPtr tagCollections = adoptCF(CFArrayCreateMutable(kCFAllocatorDefault, bufferGroupCount, &kCFTypeArrayCallBacks));

    for (CFIndex index = 0; index < bufferGroupCount; ++index) {
        CMTag videoLayerIDTag = PAL::kCMTagInvalid;
        CFIndex copiedTags = kCFNotFound;
        RetainPtr tagCollection = PAL::CMTaggedBufferGroupGetTagCollectionAtIndex(bufferGroup, index);
        if (!tagCollection)
            return nullptr;

        PAL::CMTagCollectionGetTagsWithCategory(tagCollection.get(), kCMTagCategory_VideoLayerID, &videoLayerIDTag, 1, &copiedTags);
        if (copiedTags != 1 || PAL::CMTagEqualToTag(PAL::kCMTagInvalid, videoLayerIDTag)) {
            RELEASE_LOG_ERROR(Media, "Unable to retrieve CMTagCollection");
            return nullptr;
        }
        int64_t videoLayerValue = CMTagGetValue(videoLayerIDTag);
        auto it = layerIDToStereoViewTable.find(videoLayerValue);
        if (it == layerIDToStereoViewTable.end()) {
            RELEASE_LOG_ERROR(Media, "Unable to find stereoView mapping for layer:%lld", videoLayerValue);
            return nullptr;
        }
        CMTag stereoView = PAL::CMTagMakeWithFlagsValue(kCMTagCategory_StereoView, it->second);

        CMTag refinedTags[] = { PAL::kCMTagMediaTypeVideo, videoLayerIDTag, stereoView };
        CMTagCollectionRef rawRefinedTagCollection = nullptr;
        if (auto status = PAL::CMTagCollectionCreate(kCFAllocatorDefault, refinedTags, sizeof(refinedTags) / sizeof(refinedTags[0]), &rawRefinedTagCollection); status != noErr) {
            RELEASE_LOG_ERROR(Media, "Unable to allocate CMTagCollection for layer:%lld with error:%d", videoLayerValue, int(status));
            return nullptr;
        }
        RetainPtr refinedTagCollection = adoptCF(rawRefinedTagCollection);
        CFArrayAppendValue(tagCollections.get(), refinedTagCollection.get());
    }
    if (!CFArrayGetCount(tagCollections.get()))
        return nullptr;

    return tagCollections;
}

static RetainPtr<CMTaggedBufferGroupRef> createTaggedBufferGroupWithRequiredVideoTagsFromVideoToolboxOutput(CMTaggedBufferGroupRef bufferGroup, CFArrayRef tagCollections)
{
    ASSERT(bufferGroup);

    CFIndex bufferGroupCount = PAL::CMTaggedBufferGroupGetCount(bufferGroup);

    if (CFArrayGetCount(tagCollections) != bufferGroupCount) {
        RELEASE_LOG_ERROR(Media, "Mismatch between tag collection count and buffer group.");
        return nullptr;
    }

    RetainPtr buffers = adoptCF(CFArrayCreateMutable(kCFAllocatorDefault, bufferGroupCount, &kCFTypeArrayCallBacks));
    for (CFIndex index = 0; index < bufferGroupCount; ++index) {
        RetainPtr pbuf = PAL::CMTaggedBufferGroupGetCVPixelBufferAtIndex(bufferGroup, index);
        if (!pbuf)
            return nullptr;
        CFArrayAppendValue(buffers.get(), pbuf.get());
    }

    CMTaggedBufferGroupRef refinedTaggedBufferGroup = nullptr;
    if (auto status = PAL::CMTaggedBufferGroupCreate(kCFAllocatorDefault, tagCollections, buffers.get(), &refinedTaggedBufferGroup); status != noErr) {
        RELEASE_LOG_ERROR(Media, "Unable to allocate CMTaggedBufferGroup with error:%d", int(status));
        return bufferGroup;
    }
    return adoptCF(refinedTaggedBufferGroup);
}

Expected<RetainPtr<VTDecompressionSessionRef>, OSStatus> WebCoreDecompressionSession::ensureDecompressionSessionForSample(CMSampleBufferRef cmSample)
{
    Locker lock { m_lock };

    if (isInvalidated())
        return makeUnexpected(kVTInvalidSessionErr);

    RetainPtr videoFormatDescription = PAL::CMSampleBufferGetFormatDescription(cmSample);
    bool videoFormatDescriptionChanged = !m_lastFormatDescription || !PAL::CMFormatDescriptionEqual(videoFormatDescription.get(), m_lastFormatDescription.get());

    if (videoFormatDescriptionChanged && m_videoDecoder)
        std::exchange(m_videoDecoder, nullptr)->close();

    if (m_videoDecoder)
        return RetainPtr<VTDecompressionSessionRef> { };

    if (m_decompressionSession && videoFormatDescriptionChanged && !VTDecompressionSessionCanAcceptFormatDescription(m_decompressionSession.get(), videoFormatDescription.get())) {
        auto status = VTDecompressionSessionWaitForAsynchronousFrames(m_decompressionSession.get());
        Ref sample = MediaSampleAVFObjC::create(cmSample, 0);
        m_decompressionSession = nullptr;
        m_isHardwareAccelerated.reset();
        if (!(sample->flags() & MediaSample::IsSync)) {
            RELEASE_LOG_INFO(Media, "VTDecompressionSession can't accept format description change on non-keyframe status:%d", int(status));
            return makeUnexpected(status == kVTInvalidSessionErr ? status : kVTVideoDecoderBadDataErr);
        }
        RELEASE_LOG_INFO(Media, "VTDecompressionSession can't accept format description change on keyframe, creating new VTDecompressionSession status:%d", int(status));
    }
    m_lastFormatDescription = videoFormatDescription;

    if (!m_decompressionSession) {
        auto videoDecoderSpecification = @{ (__bridge NSString *)kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder: @YES };
        ASSERT(m_pixelBufferAttributes);

        VTDecompressionSessionRef decompressionSessionOut = nullptr;
        auto result = VTDecompressionSessionCreate(kCFAllocatorDefault, videoFormatDescription.get(), (__bridge CFDictionaryRef)videoDecoderSpecification, (__bridge CFDictionaryRef)m_pixelBufferAttributes.get(), nullptr, &decompressionSessionOut);
        if (noErr == result)
            m_decompressionSession = adoptCF(decompressionSessionOut);
        if (m_dispatcher->isCurrent()) {
            assertIsCurrent(m_dispatcher.get());

            m_currentImageDescription = nullptr;
            m_stereoConfigured = false;
            m_tagCollections = nullptr;
        }
    }

    return m_decompressionSession;
}

static bool isNonRecoverableError(OSStatus status)
{
    return status != noErr && status != kVTVideoDecoderReferenceMissingErr;
}

static RetainPtr<CMFormatDescriptionRef> copyDescriptionExtensionValuesIfNeeded(RetainPtr<CMFormatDescriptionRef>&& description, const CMVideoFormatDescriptionRef originalDescription, const CMTaggedBufferGroupRef group)
{
    if (!canCopyFormatDescriptionExtension())
        return description;

    static CFStringRef keys[] = {
        PAL::kCMFormatDescriptionExtension_CameraCalibrationDataLensCollection,
        PAL::kCMFormatDescriptionExtension_HasLeftStereoEyeView,
        PAL::kCMFormatDescriptionExtension_HasRightStereoEyeView,
        PAL::kCMFormatDescriptionExtension_HeroEye,
        PAL::kCMFormatDescriptionExtension_HorizontalFieldOfView,
        PAL::kCMFormatDescriptionExtension_HorizontalDisparityAdjustment,
        PAL::kCMFormatDescriptionExtension_StereoCameraBaseline,
        PAL::kCMFormatDescriptionExtension_ProjectionKind,
        PAL::kCMFormatDescriptionExtension_ViewPackingKind
    };
    static constexpr size_t numberOfKeys = sizeof(keys) / sizeof(keys[0]);
    auto keysSpan = unsafeMakeSpan(keys, numberOfKeys);
    size_t keysSet = 0;
    Vector<RetainPtr<CFPropertyListRef>, numberOfKeys> values(numberOfKeys, [&](size_t index) -> RetainPtr<CFPropertyListRef> {
        RetainPtr value = PAL::CMFormatDescriptionGetExtension(originalDescription, RetainPtr { keysSpan[index] }.get());
        if (!value)
            return nullptr;
        keysSet++;
        return value;
    });

    if (!keysSet)
        return description;

    RetainPtr<CFMutableDictionaryRef> copyExtensions;
    if (RetainPtr extensions = PAL::CMFormatDescriptionGetExtensions(description.get()))
        copyExtensions = adoptCF(CFDictionaryCreateMutableCopy(kCFAllocatorDefault, CFDictionaryGetCount(extensions.get()) + keysSet, extensions.get()));
    else
        copyExtensions = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, keysSet, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    for (size_t index = 0; index < numberOfKeys; index++) {
        if (values[index])
            CFDictionarySetValue(copyExtensions.get(), keysSpan[index], values[index].get());
    }

    if (group) {
        CMVideoFormatDescriptionRef newTaggedGroupDescription;
        if (auto status = PAL::CMTaggedBufferGroupFormatDescriptionCreateForTaggedBufferGroupWithExtensions(kCFAllocatorDefault, group, copyExtensions.get(), &newTaggedGroupDescription); status != noErr)
            return description;
        return adoptCF(newTaggedGroupDescription);
    }
    auto codecType = PAL::CMFormatDescriptionGetMediaSubType(description.get());
    auto dimensions = PAL::CMVideoFormatDescriptionGetDimensions(description.get());

    CMVideoFormatDescriptionRef newImageDescription;
    if (auto status = PAL::CMVideoFormatDescriptionCreate(kCFAllocatorDefault, codecType, dimensions.width, dimensions.height, copyExtensions.get(), &newImageDescription); status != noErr)
        return description;
    return adoptCF(newImageDescription);
}

static Expected<RetainPtr<CMSampleBufferRef>, OSStatus> handleDecompressionOutput(WebCoreDecompressionSession::DecodingFlags flags, OSStatus status, VTDecodeInfoFlags, CVImageBufferRef imageBuffer, CMTaggedBufferGroupRef group, CMTime presentationTimeStamp, CMTime presentationDuration, CMFormatDescriptionRef currentImageDescription, CMVideoFormatDescriptionRef description = nullptr)
{
    if (isNonRecoverableError(status)) {
        RELEASE_LOG_ERROR(Media, "Video sample decompression failed with error:%d", int(status));
        return makeUnexpected(status);
    }

    if (flags.contains(WebCoreDecompressionSession::DecodingFlag::NonDisplaying) || (!imageBuffer && !group))
        return { };

    if (group) {
        RetainPtr groupFormatDescription = currentImageDescription;
        if (!groupFormatDescription) {
            CMTaggedBufferGroupFormatDescriptionRef rawBufferGroupFormatDescription = nullptr;
            if (auto status = PAL::CMTaggedBufferGroupFormatDescriptionCreateForTaggedBufferGroup(kCFAllocatorDefault, group, &rawBufferGroupFormatDescription); status != noErr)
                return makeUnexpected(status);
            groupFormatDescription = copyDescriptionExtensionValuesIfNeeded(adoptCF(rawBufferGroupFormatDescription), description, group);
        }
        ASSERT(PAL::CMFormatDescriptionGetMediaType(groupFormatDescription.get()) == kCMMediaType_TaggedBufferGroup);
        CMSampleBufferRef rawGroupSampleBuffer;
        if (auto status = PAL::CMSampleBufferCreateForTaggedBufferGroup(kCFAllocatorDefault, group, presentationTimeStamp, presentationDuration, groupFormatDescription.get(), &rawGroupSampleBuffer); status != noErr)
            return makeUnexpected(status);
        return adoptCF(rawGroupSampleBuffer);
    }

    RetainPtr imageBufferDescription = currentImageDescription;
    if (!imageBufferDescription) {
        CMVideoFormatDescriptionRef rawImageBufferDescription = nullptr;
        if (auto status = PAL::CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, imageBuffer, &rawImageBufferDescription); status != noErr)
            return makeUnexpected(status);
        imageBufferDescription = copyDescriptionExtensionValuesIfNeeded(adoptCF(rawImageBufferDescription), description, nullptr);
    }
    ASSERT(PAL::CMFormatDescriptionGetMediaType(imageBufferDescription.get()) == kCMMediaType_Video);
    CMSampleTimingInfo imageBufferTiming {
        presentationDuration,
        presentationTimeStamp,
        presentationTimeStamp,
    };

    CMSampleBufferRef rawImageSampleBuffer = nullptr;
    if (auto status = PAL::CMSampleBufferCreateReadyWithImageBuffer(kCFAllocatorDefault, imageBuffer, imageBufferDescription.get(), &imageBufferTiming, &rawImageSampleBuffer); status != noErr)
        return makeUnexpected(status);

    return adoptCF(rawImageSampleBuffer);
}

auto WebCoreDecompressionSession::decodeSample(CMSampleBufferRef sample, DecodingFlags flags) -> Ref<DecodingPromise>
{
    DecodingPromise::Producer producer;
    auto promise = producer.promise();
    m_dispatcher->dispatch([protectedThis = RefPtr { this }, producer = WTFMove(producer), sample = RetainPtr { sample }, flags, flushId = m_flushId.load()]() mutable {
        if (flushId == protectedThis->m_flushId)
            protectedThis->decodeSampleInternal(sample.get(), flags)->chainTo(WTFMove(producer));
        else
            producer.reject(noErr);
    });
    return promise;
}

Ref<WebCoreDecompressionSession::DecodingPromise> WebCoreDecompressionSession::decodeSampleInternal(CMSampleBufferRef sample, DecodingFlags flags)
{
    assertIsCurrent(m_dispatcher.get());

    m_lastDecodingError = noErr;
    size_t numberOfSamples = PAL::CMSampleBufferGetNumSamples(sample);
    m_lastDecodedSamples = { };
    m_lastDecodedSamples.reserveInitialCapacity(numberOfSamples);

    auto result = ensureDecompressionSessionForSample(sample);
    if (!result)
        return DecodingPromise::createAndReject(result.error());
    RetainPtr decompressionSession = WTFMove(*result);

    if (!decompressionSession && !m_videoDecoderCreationFailed) {
        RefPtr<MediaPromise> initPromise;

        {
            Locker lock { m_lock };
            if (!m_videoDecoder) {
                if (isInvalidated())
                    return DecodingPromise::createAndReject(0);
                RetainPtr videoFormatDescription = PAL::CMSampleBufferGetFormatDescription(sample);
                auto fourCC = PAL::CMFormatDescriptionGetMediaSubType(videoFormatDescription.get());

                RetainPtr extensions = PAL::CMFormatDescriptionGetExtensions(videoFormatDescription.get());
                if (!extensions)
                    return DecodingPromise::createAndReject(0);

                RetainPtr extensionAtoms = dynamic_cf_cast<CFDictionaryRef>(CFDictionaryGetValue(extensions.get(), PAL::kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms));
                if (!extensionAtoms)
                    return DecodingPromise::createAndReject(0);

                // We should only hit this code path for VP8 and SW VP9 decoder, look for the vpcC path.
                RetainPtr configurationRecord = dynamic_cf_cast<CFDataRef>(CFDictionaryGetValue(extensionAtoms.get(), CFSTR("vpcC")));
                if (!configurationRecord)
                    return DecodingPromise::createAndReject(0);

                auto colorSpace = colorSpaceFromFormatDescription(videoFormatDescription.get());

                initPromise = initializeVideoDecoder(fourCC, span(configurationRecord.get()), colorSpace);
            }
        }
        auto decode = [protectedThis = Ref { *this }, this, cmSamples = RetainPtr { sample }, flags] {
            Locker lock { m_lock };
            RefPtr videoDecoder = m_videoDecoder;
            if (!videoDecoder)
                return DecodingPromise::createAndReject(0);

            assertIsCurrent(m_dispatcher.get());

            m_pendingDecodeData = { flags };
            MediaTime totalDuration = PAL::toMediaTime(PAL::CMSampleBufferGetDuration(cmSamples.get()));

            Vector<Ref<VideoDecoder::DecodePromise>> promises;
            for (Ref sample : MediaSampleAVFObjC::create(cmSamples.get(), 0)->divide()) {
                RetainPtr cmSample = sample->platformSample().cmSampleBuffer();
                MediaTime presentationTimestamp = PAL::toMediaTime(PAL::CMSampleBufferGetPresentationTimeStamp(cmSample.get()));
                RetainPtr rawBuffer = PAL::CMSampleBufferGetDataBuffer(cmSample.get());
                ASSERT(rawBuffer);
                RetainPtr buffer = rawBuffer;
                // Make sure block buffer is contiguous.
                if (!PAL::CMBlockBufferIsRangeContiguous(rawBuffer.get(), 0, 0)) {
                    CMBlockBufferRef contiguousBuffer;
                    if (auto status = PAL::CMBlockBufferCreateContiguous(nullptr, rawBuffer.get(), nullptr, nullptr, 0, 0, 0, &contiguousBuffer))
                        return DecodingPromise::createAndReject(status);
                    buffer = adoptCF(contiguousBuffer);
                }
                auto data = PAL::CMBlockBufferGetDataSpan(buffer.get());
                if (!data.data())
                    return DecodingPromise::createAndReject(-1);
                promises.append(videoDecoder->decode({ data, true, presentationTimestamp.toMicroseconds(), 0 }));
            }
            DecodingPromise::Producer producer;
            auto promise = producer.promise();
            VideoDecoder::DecodePromise::all(promises)->whenSettled(m_dispatcher, [weakThis = ThreadSafeWeakPtr { *this }, totalDuration = PAL::toCMTime(totalDuration), producer = WTFMove(producer)] (auto&& result) {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis || protectedThis->isInvalidated()) {
                    producer.reject(0);
                    return;
                }
                assertIsCurrent(protectedThis->m_dispatcher.get());
                if (protectedThis->m_lastDecodingError)
                    producer.reject(protectedThis->m_lastDecodingError);
                else if (!result)
                    producer.reject(kVTVideoDecoderBadDataErr);
                else
                    producer.resolve(std::exchange(protectedThis->m_lastDecodedSamples, { }));
                if (!protectedThis->m_pendingDecodeData)
                    return;
                protectedThis->m_pendingDecodeData.reset();
            });
            return promise;
        };
        if (initPromise) {
            return initPromise->then(m_dispatcher, WTFMove(decode), [] {
                return DecodingPromise::createAndReject(kVTVideoDecoderNotAvailableNowErr);
            });
        }
        return decode();
    }

    if (!decompressionSession)
        return DecodingPromise::createAndReject(kVTVideoDecoderNotAvailableNowErr);

    DecodingPromise::Producer producer;
    auto promise = producer.promise();

    auto handler = makeBlockPtr([weakThis = ThreadSafeWeakPtr { *this }, flags, producer = WTFMove(producer), numberOfTimesCalled = 0u, numberOfSamples, decodedSamples = std::exchange(m_lastDecodedSamples, { }), originalDescription = RetainPtr { PAL::CMSampleBufferGetFormatDescription(sample) }, currentImageDescription = m_currentImageDescription, tagCollections = m_tagCollections, workQueue = Ref { m_dispatcher }](OSStatus status, VTDecodeInfoFlags infoFlags, CVImageBufferRef imageBuffer, CMTaggedBufferGroupRef taggedBufferGroup, CMTime presentationTimeStamp, CMTime presentationDuration) mutable {
        if (producer.isSettled())
            return;
        RetainPtr group = taggedBufferGroup;
        if (taggedBufferGroup) {
            if (!tagCollections) {
                auto map = createMVHEVCStereoViewMap(originalDescription.get());
                tagCollections = createTagCollectionRequiredFromVideoToolboxOutput(taggedBufferGroup, map);
                if (!tagCollections) {
                    producer.reject(kVTCouldNotOutputTaggedBufferGroupErr);
                    return;
                }
                workQueue->dispatch([weakThis, tagCollections]() mutable {
                    if (RefPtr protectedThis = weakThis.get()) {
                        assertIsCurrent(protectedThis->m_dispatcher.get());
                        protectedThis->m_tagCollections = WTFMove(tagCollections);
                    }
                });
            }
            group = createTaggedBufferGroupWithRequiredVideoTagsFromVideoToolboxOutput(taggedBufferGroup, tagCollections.get());
        }
        auto result = handleDecompressionOutput(flags, status, infoFlags, imageBuffer, group.get(), presentationTimeStamp, presentationDuration, currentImageDescription.get(), originalDescription.get());
        if (!result) {
            producer.reject(result.error());
            return;
        }
        if (!currentImageDescription) {
            currentImageDescription = PAL::CMSampleBufferGetFormatDescription(result->get());
            workQueue->dispatch([weakThis, currentImageDescription]() mutable {
                if (RefPtr protectedThis = weakThis.get()) {
                    assertIsCurrent(protectedThis->m_dispatcher.get());
                    protectedThis->m_currentImageDescription = WTFMove(currentImageDescription);
                }
            });
        }
        decodedSamples.append(WTFMove(*result));
        if (++numberOfTimesCalled == numberOfSamples)
            producer.resolve(std::exchange(decodedSamples, { }));
    });

    VTDecodeInfoFlags decodeInfoFlags = kVTDecodeFrame_EnableAsynchronousDecompression | kVTDecodeFrame_EnableTemporalProcessing;
    if (flags.contains(DecodingFlag::NonDisplaying))
        decodeInfoFlags |= kVTDecodeFrame_DoNotOutputFrame;
    if (flags.contains(DecodingFlag::RealTime))
        decodeInfoFlags |= kVTDecodeFrame_1xRealTimePlayback;

    if (m_stereoSupported && flags.contains(DecodingFlag::EnableStereo) && !m_stereoConfigured) {
        m_stereoConfigured = true;
        RetainPtr videoFormatDescription = PAL::CMSampleBufferGetFormatDescription(sample);
        if (RetainPtr layers = createMVHEVCVideoLayersArray(videoFormatDescription.get()))
            VTSessionSetProperty(decompressionSession.get(), kVTDecompressionPropertyKey_RequestedMVHEVCVideoLayerIDs, layers.get());
    }

    if (auto result = VTDecompressionSessionDecodeFrameWithMultiImageCapableOutputHandler(decompressionSession.get(), sample, decodeInfoFlags, nullptr, handler.get()); result != noErr)
        handler(result, 0, nullptr, nullptr, PAL::kCMTimeInvalid, PAL::kCMTimeInvalid); // If VTDecompressionSessionDecodeFrameWithOutputHandler returned an error, the handler would not have been called.

    return promise;
}

RetainPtr<CVPixelBufferRef> WebCoreDecompressionSession::decodeSampleSync(CMSampleBufferRef sample)
{
    auto result = ensureDecompressionSessionForSample(sample);
    if (!result || !*result)
        return nullptr;

    RetainPtr decompressionSession = WTFMove(*result);
    RetainPtr<CVPixelBufferRef> pixelBuffer;
    VTDecodeInfoFlags flags { 0 };
    WTF::Semaphore syncDecompressionOutputSemaphore { 0 };
    Ref protectedThis { *this };
    VTDecompressionSessionDecodeFrameWithOutputHandler(decompressionSession.get(), sample, flags, nullptr, [&protectedThis, &pixelBuffer, &syncDecompressionOutputSemaphore] (OSStatus, VTDecodeInfoFlags, CVImageBufferRef imageBuffer, CMTime, CMTime) mutable {
        protectedThis->assignResourceOwner(imageBuffer);
        if (imageBuffer && CFGetTypeID(imageBuffer) == CVPixelBufferGetTypeID())
            pixelBuffer = (CVPixelBufferRef)imageBuffer;
        syncDecompressionOutputSemaphore.signal();
    });
    syncDecompressionOutputSemaphore.wait();
    return pixelBuffer;
}

void WebCoreDecompressionSession::flush()
{
    m_flushId++;
}

Ref<MediaPromise> WebCoreDecompressionSession::initializeVideoDecoder(FourCharCode codec, std::span<const uint8_t> description, const std::optional<PlatformVideoColorSpace>& colorSpace)
{
    VideoDecoder::Config config {
        .description = description,
        .colorSpace = colorSpace,
        .decoding = VideoDecoder::HardwareAcceleration::Yes,
        .pixelBuffer = VideoDecoder::HardwareBuffer::Yes,
        .noOutputAsError = VideoDecoder::TreatNoOutputAsError::No
    };
    MediaPromise::Producer producer;
    auto promise = producer.promise();

    VideoDecoder::create(VideoDecoder::fourCCToCodecString(codec), config, [weakThis = ThreadSafeWeakPtr { *this }, workQueue = Ref { m_dispatcher }] (auto&& result) {
        workQueue->dispatch([weakThis, result = WTFMove(result)] () {
            if (RefPtr protectedThis = weakThis.get()) {
                assertIsCurrent(protectedThis->m_dispatcher.get());
                if (protectedThis->isInvalidated() || !protectedThis->m_pendingDecodeData)
                    return;

                if (!result) {
                    protectedThis->m_lastDecodingError = -1;
                    return;
                }
                if (protectedThis->m_lastDecodingError)
                    return;

                auto presentationTime = PAL::toCMTime(MediaTime(result->timestamp, 1000000));
                auto presentationDuration = PAL::toCMTime(MediaTime(result->duration.value_or(0), 1000000));
                auto sampleResult = handleDecompressionOutput(protectedThis->m_pendingDecodeData->flags, noErr, 0, result->frame->pixelBuffer(), nullptr, presentationTime, presentationDuration, nullptr);
                if (!sampleResult)
                    protectedThis->m_lastDecodingError = sampleResult.error();
                else
                    protectedThis->m_lastDecodedSamples.append(WTFMove(*sampleResult));
            }
        });
    })->whenSettled(m_dispatcher, [protectedThis = Ref { *this }, this, producer = WTFMove(producer)] (auto&& result) mutable {
        assertIsCurrent(m_dispatcher.get());
        if (!result || isInvalidated()) {
            producer.reject(PlatformMediaError::DecoderCreationError);
            return;
        }
        Locker lock { m_lock };
        m_videoDecoder = WTFMove(*result);
        producer.resolve();
    });

    return promise;
}

bool WebCoreDecompressionSession::isHardwareAccelerated() const
{
    Locker lock { m_lock };
    if (m_videoDecoder)
        return false;
    if (m_isHardwareAccelerated)
        return *m_isHardwareAccelerated;
    if (!m_decompressionSession)
        return false;
    CFBooleanRef isHardwareAccelerated = NULL;
    VTSessionCopyProperty(m_decompressionSession.get(), kVTDecompressionPropertyKey_UsingHardwareAcceleratedVideoDecoder, kCFAllocatorDefault, &isHardwareAccelerated);
    m_isHardwareAccelerated = isHardwareAccelerated && isHardwareAccelerated == kCFBooleanTrue;
    return *m_isHardwareAccelerated;
}

} // namespace WebCore
