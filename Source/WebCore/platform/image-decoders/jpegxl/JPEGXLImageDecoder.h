/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ScalableImageDecoder.h"

#if USE(JPEGXL)

#include "JxlDecoderPtr.h"

#if USE(LCMS)
#include "LCMSUniquePtr.h"
#elif USE(CG)
#include <CoreGraphics/CoreGraphics.h>
#include <wtf/RetainPtr.h>
#endif

namespace WebCore {

// This class decodes the JPEG XL image format.
class JPEGXLImageDecoder final : public ScalableImageDecoder {
public:
    static RefPtr<ScalableImageDecoder> create(AlphaOption, GammaAndColorProfileOption);

    virtual ~JPEGXLImageDecoder();

    // ScalableImageDecoder
    String filenameExtension() const override { return "jxl"_s; }
    size_t frameCount() const override WTF_REQUIRES_LOCK(m_lock);
    RepetitionCount repetitionCount() const override;
    ScalableImageDecoderFrame* frameBufferAtIndex(size_t index) override WTF_REQUIRES_LOCK(m_lock);
    void clearFrameBufferCache(size_t clearBeforeFrame) override WTF_REQUIRES_LOCK(m_lock);

    bool setFailed() override;

private:
    enum class Query {
        // This query is used for tryDecodeSize().
        Size,
        // We define a query for frame count because JPEG XL doesn't have frame count information in its code stream
        // so we need to scan the code stream to get the frame count for animated JPEG XL.
        // JPEG XL container can have frame count metadata but currently libjxl doesn't support it.
        FrameCount,
        // Query to decode a single frame.
        DecodedImage,
    };

    JPEGXLImageDecoder(AlphaOption, GammaAndColorProfileOption);

    void clear();

    void tryDecodeSize(bool allDataReceived) override WTF_REQUIRES_LOCK(m_lock);

    bool hasAlpha() const;
    bool hasAnimation() const;

    void ensureDecoderInitialized();
    bool shouldRewind(Query , size_t frameIndex) const;
    void rewind();
    void updateFrameCount() WTF_REQUIRES_LOCK(m_lock);

    void decode(Query, size_t frameIndex, bool allDataReceived) WTF_REQUIRES_LOCK(m_lock);
    JxlDecoderStatus processInput(Query) WTF_REQUIRES_LOCK(m_lock);
    static void imageOutCallback(void*, size_t x, size_t y, size_t numPixels, const void* pixels);
    void imageOut(size_t x, size_t y, size_t numPixels, const uint8_t* pixels) WTF_REQUIRES_LOCK(m_lock);

    void clearColorTransform();
    void prepareColorTransform();
    void maybePerformColorSpaceConversion(void* inputBuffer, void* outputBuffer, unsigned numberOfPixels);
#if USE(LCMS)
    LCMSProfilePtr tryDecodeICCColorProfile();
#elif USE(CG)
    RetainPtr<CGColorSpaceRef> tryDecodeICCColorProfile();
#endif

    JxlDecoderPtr m_decoder;
    size_t m_readOffset { 0 };
    std::optional<JxlBasicInfo> m_basicInfo;

    Query m_lastQuery { Query::Size };
    size_t m_frameCount { 1 };
    size_t m_currentFrame { 0 };

    bool m_isLastFrameHeaderReceived { false }; // If this is true, we know we don't need to update m_frameCount.

#if USE(LCMS)
    LCMSTransformPtr m_iccTransform;
#elif USE(CG)
    RetainPtr<CGColorSpaceRef> m_profile;
#endif
};

} // namespace WebCore

#endif // USE(JPEGXL)
