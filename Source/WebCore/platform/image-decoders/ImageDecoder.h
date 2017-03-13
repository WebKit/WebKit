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

#include "ImageFrame.h"
#include "IntRect.h"
#include "IntSize.h"
#include "PlatformScreen.h"
#include "SharedBuffer.h"
#include <wtf/Assertions.h>
#include <wtf/Optional.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

    // ImageDecoder is a base for all format-specific decoders
    // (e.g. JPEGImageDecoder).  This base manages the ImageFrame cache.
    //
    // ENABLE(IMAGE_DECODER_DOWN_SAMPLING) allows image decoders to downsample
    // at decode time.  Image decoders will downsample any images larger than
    // |m_maxNumPixels|.  FIXME: Not yet supported by all decoders.
    class ImageDecoder : public RefCounted<ImageDecoder> {
        WTF_MAKE_NONCOPYABLE(ImageDecoder); WTF_MAKE_FAST_ALLOCATED;
    public:
        ImageDecoder(AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
            : m_premultiplyAlpha(alphaOption == AlphaOption::Premultiplied)
            , m_ignoreGammaAndColorProfile(gammaAndColorProfileOption == GammaAndColorProfileOption::Ignored)
        {
        }

        virtual ~ImageDecoder()
        {
        }

        // Returns nullptr if we can't sniff a supported type from the provided data (possibly
        // because there isn't enough data yet).
        static RefPtr<ImageDecoder> create(const SharedBuffer& data, AlphaOption, GammaAndColorProfileOption);

        virtual String filenameExtension() const = 0;
        
        bool premultiplyAlpha() const { return m_premultiplyAlpha; }

        bool isAllDataReceived() const { return m_isAllDataReceived; }

        virtual void setData(SharedBuffer& data, bool allDataReceived)
        {
            if (m_failed)
                return;
            m_data = &data;
            m_isAllDataReceived = allDataReceived;
        }

        // Lazily-decodes enough of the image to get the size (if possible).
        // FIXME: Right now that has to be done by each subclass; factor the
        // decode call out and use it here.
        virtual bool isSizeAvailable()
        {
            return !m_failed && m_sizeAvailable;
        }

        virtual IntSize size() { return isSizeAvailable() ? m_size : IntSize(); }

        IntSize scaledSize()
        {
            return m_scaled ? IntSize(m_scaledColumns.size(), m_scaledRows.size()) : size();
        }

        // This will only differ from size() for ICO (where each frame is a
        // different icon) or other formats where different frames are different
        // sizes.  This does NOT differ from size() for GIF, since decoding GIFs
        // composites any smaller frames against previous frames to create full-
        // size frames.
        virtual IntSize frameSizeAtIndex(size_t, SubsamplingLevel)
        {
            return size();
        }

        // Returns whether the size is legal (i.e. not going to result in
        // overflow elsewhere).  If not, marks decoding as failed.
        virtual bool setSize(const IntSize& size)
        {
            if (ImageBackingStore::isOverSize(size))
                return setFailed();
            m_size = size;
            m_sizeAvailable = true;
            return true;
        }

        // Lazily-decodes enough of the image to get the frame count (if
        // possible), without decoding the individual frames.
        // FIXME: Right now that has to be done by each subclass; factor the
        // decode call out and use it here.
        virtual size_t frameCount() const { return 1; }

        virtual RepetitionCount repetitionCount() const { return RepetitionCountNone; }

        // Decodes as much of the requested frame as possible, and returns an
        // ImageDecoder-owned pointer.
        virtual ImageFrame* frameBufferAtIndex(size_t) = 0;

        bool frameIsCompleteAtIndex(size_t);

        // Make the best effort guess to check if the requested frame has alpha channel.
        bool frameHasAlphaAtIndex(size_t) const;

        // Number of bytes in the decoded frame requested. Return 0 if not yet decoded.
        unsigned frameBytesAtIndex(size_t) const;
        
        float frameDurationAtIndex(size_t);
        
        NativeImagePtr createFrameImageAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default, const std::optional<IntSize>& sizeForDraw = { });

        void setIgnoreGammaAndColorProfile(bool flag) { m_ignoreGammaAndColorProfile = flag; }
        bool ignoresGammaAndColorProfile() const { return m_ignoreGammaAndColorProfile; }

        ImageOrientation frameOrientationAtIndex(size_t) const { return m_orientation; }
        
        bool frameAllowSubsamplingAtIndex(size_t) const { return false; }

        enum { iccColorProfileHeaderLength = 128 };

        static bool rgbColorProfile(const char* profileData, unsigned profileLength)
        {
            ASSERT_UNUSED(profileLength, profileLength >= iccColorProfileHeaderLength);

            return !memcmp(&profileData[16], "RGB ", 4);
        }

        static size_t bytesDecodedToDetermineProperties() { return 0; }
        
        static SubsamplingLevel subsamplingLevelForScale(float, SubsamplingLevel) { return SubsamplingLevel::Default; }

        static bool inputDeviceColorProfile(const char* profileData, unsigned profileLength)
        {
            ASSERT_UNUSED(profileLength, profileLength >= iccColorProfileHeaderLength);

            return !memcmp(&profileData[12], "mntr", 4) || !memcmp(&profileData[12], "scnr", 4);
        }

        // Sets the "decode failure" flag.  For caller convenience (since so
        // many callers want to return false after calling this), returns false
        // to enable easy tailcalling.  Subclasses may override this to also
        // clean up any local data.
        virtual bool setFailed()
        {
            m_failed = true;
            return false;
        }

        bool failed() const { return m_failed; }

        // Clears decoded pixel data from before the provided frame unless that
        // data may be needed to decode future frames (e.g. due to GIF frame
        // compositing).
        virtual void clearFrameBufferCache(size_t) { }

        // If the image has a cursor hot-spot, stores it in the argument
        // and returns true. Otherwise returns false.
        virtual std::optional<IntPoint> hotSpot() const { return std::nullopt; }

    protected:
        void prepareScaleDataIfNecessary();
        int upperBoundScaledX(int origX, int searchStart = 0);
        int lowerBoundScaledX(int origX, int searchStart = 0);
        int upperBoundScaledY(int origY, int searchStart = 0);
        int lowerBoundScaledY(int origY, int searchStart = 0);
        int scaledY(int origY, int searchStart = 0);

        RefPtr<SharedBuffer> m_data; // The encoded data.
        Vector<ImageFrame, 1> m_frameBufferCache;
        bool m_scaled { false };
        Vector<int> m_scaledColumns;
        Vector<int> m_scaledRows;
        bool m_premultiplyAlpha;
        bool m_ignoreGammaAndColorProfile;
        ImageOrientation m_orientation;

    private:
        IntSize m_size;
        bool m_sizeAvailable { false };
#if ENABLE(IMAGE_DECODER_DOWN_SAMPLING)
        static const int m_maxNumPixels { 1024 * 1024 };
#else
        static const int m_maxNumPixels { -1 };
#endif
        bool m_isAllDataReceived { false };
        bool m_failed { false };
    };

} // namespace WebCore
