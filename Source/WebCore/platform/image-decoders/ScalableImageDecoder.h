/*
 * Copyright (C) 2006, 2016 Apple Inc.  All rights reserved.
 * Copyright (C) 2008-2009 Torch Mobile, Inc.
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ImageDecoder.h"
#include "IntRect.h"
#include "ScalableImageDecoderFrame.h"
#include "SharedBuffer.h"
#include <wtf/Assertions.h>
#include <wtf/Lock.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

// ScalableImageDecoder is a base for all format-specific decoders
// (e.g. JPEGImageDecoder). This base manages the ScalableImageDecoderFrame cache.

class ScalableImageDecoder : public ImageDecoder {
    WTF_MAKE_NONCOPYABLE(ScalableImageDecoder); WTF_MAKE_FAST_ALLOCATED;
public:
    ScalableImageDecoder(AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
        : m_premultiplyAlpha(alphaOption == AlphaOption::Premultiplied)
        , m_ignoreGammaAndColorProfile(gammaAndColorProfileOption == GammaAndColorProfileOption::Ignored)
    {
    }

    virtual ~ScalableImageDecoder()
    {
    }

    static bool supportsMediaType(MediaType type) { return type == MediaType::Image; }

    // Returns nullptr if we can't sniff a supported type from the provided data (possibly
    // because there isn't enough data yet).
    static RefPtr<ScalableImageDecoder> create(SharedBuffer& data, AlphaOption, GammaAndColorProfileOption);

    bool premultiplyAlpha() const { return m_premultiplyAlpha; }

    bool isAllDataReceived() const override
    {
        ASSERT(!m_decodingSizeFromSetData);
        return m_encodedDataStatus == EncodedDataStatus::Complete;
    }

    void setData(SharedBuffer& data, bool allDataReceived) override
    {
        LockHolder lockHolder(m_mutex);
        if (m_encodedDataStatus == EncodedDataStatus::Error)
            return;

        if (data.data()) {
            // SharedBuffer::data() combines all segments into one in case there's more than one.
            m_data = data.begin()->segment.copyRef();
        }
        if (m_encodedDataStatus == EncodedDataStatus::TypeAvailable) {
            m_decodingSizeFromSetData = true;
            tryDecodeSize(allDataReceived);
            m_decodingSizeFromSetData = false;
        }

        if (m_encodedDataStatus == EncodedDataStatus::Error)
            return;

        if (allDataReceived) {
            ASSERT(m_encodedDataStatus == EncodedDataStatus::SizeAvailable);
            m_encodedDataStatus = EncodedDataStatus::Complete;
        }
    }

    EncodedDataStatus encodedDataStatus() const override { return m_encodedDataStatus; }

    bool isSizeAvailable() const override { return m_encodedDataStatus >= EncodedDataStatus::SizeAvailable; }

    IntSize size() const override { return isSizeAvailable() ? m_size : IntSize(); }

    // This will only differ from size() for ICO (where each frame is a
    // different icon) or other formats where different frames are different
    // sizes. This does NOT differ from size() for GIF, since decoding GIFs
    // composites any smaller frames against previous frames to create full-
    // size frames.
    IntSize frameSizeAtIndex(size_t, SubsamplingLevel) const override
    {
        return size();
    }

    // Returns whether the size is legal (i.e. not going to result in
    // overflow elsewhere). If not, marks decoding as failed.
    virtual bool setSize(const IntSize& size)
    {
        if (ImageBackingStore::isOverSize(size))
            return setFailed();
        m_size = size;
        m_encodedDataStatus = EncodedDataStatus::SizeAvailable;
        return true;
    }

    // Lazily-decodes enough of the image to get the frame count (if
    // possible), without decoding the individual frames.
    // FIXME: Right now that has to be done by each subclass; factor the
    // decode call out and use it here.
    size_t frameCount() const override { return 1; }

    RepetitionCount repetitionCount() const override { return RepetitionCountNone; }

    // Decodes as much of the requested frame as possible, and returns an
    // ScalableImageDecoder-owned pointer.
    virtual ScalableImageDecoderFrame* frameBufferAtIndex(size_t) = 0;

    bool frameIsCompleteAtIndex(size_t) const override;

    // Make the best effort guess to check if the requested frame has alpha channel.
    bool frameHasAlphaAtIndex(size_t) const override;

    // Number of bytes in the decoded frame requested. Return 0 if not yet decoded.
    unsigned frameBytesAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default) const override;

    Seconds frameDurationAtIndex(size_t) const final;

    NativeImagePtr createFrameImageAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default, const DecodingOptions& = DecodingOptions(DecodingMode::Synchronous)) override;

    void setIgnoreGammaAndColorProfile(bool flag) { m_ignoreGammaAndColorProfile = flag; }
    bool ignoresGammaAndColorProfile() const { return m_ignoreGammaAndColorProfile; }

    ImageOrientation frameOrientationAtIndex(size_t) const override { return m_orientation; }

    bool frameAllowSubsamplingAtIndex(size_t) const override { return false; }

    enum { ICCColorProfileHeaderLength = 128 };

    static bool rgbColorProfile(const char* profileData, unsigned profileLength)
    {
        ASSERT_UNUSED(profileLength, profileLength >= ICCColorProfileHeaderLength);

        return !memcmp(&profileData[16], "RGB ", 4);
    }

    size_t bytesDecodedToDetermineProperties() const final { return 0; }

    static SubsamplingLevel subsamplingLevelForScale(float, SubsamplingLevel) { return SubsamplingLevel::Default; }

    static bool inputDeviceColorProfile(const char* profileData, unsigned profileLength)
    {
        ASSERT_UNUSED(profileLength, profileLength >= ICCColorProfileHeaderLength);

        return !memcmp(&profileData[12], "mntr", 4) || !memcmp(&profileData[12], "scnr", 4);
    }

    // Sets the "decode failure" flag. For caller convenience (since so
    // many callers want to return false after calling this), returns false
    // to enable easy tailcalling. Subclasses may override this to also
    // clean up any local data.
    virtual bool setFailed()
    {
        m_encodedDataStatus = EncodedDataStatus::Error;
        return false;
    }

    bool failed() const { return m_encodedDataStatus == EncodedDataStatus::Error; }

    // Clears decoded pixel data from before the provided frame unless that
    // data may be needed to decode future frames (e.g. due to GIF frame
    // compositing).
    void clearFrameBufferCache(size_t) override { }

    // If the image has a cursor hot-spot, stores it in the argument
    // and returns true. Otherwise returns false.
    Optional<IntPoint> hotSpot() const override { return WTF::nullopt; }

protected:
    RefPtr<SharedBuffer::DataSegment> m_data;
    Vector<ScalableImageDecoderFrame, 1> m_frameBufferCache;
    mutable Lock m_mutex;
    bool m_premultiplyAlpha;
    bool m_ignoreGammaAndColorProfile;
    ImageOrientation m_orientation;

private:
    virtual void tryDecodeSize(bool) = 0;

#if USE(DIRECT2D)
    void setTargetContext(ID2D1RenderTarget*) override;
#endif

    IntSize m_size;
    EncodedDataStatus m_encodedDataStatus { EncodedDataStatus::TypeAvailable };
    bool m_decodingSizeFromSetData { false };
};

} // namespace WebCore
