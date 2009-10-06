/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2008-2009 Torch Mobile, Inc.
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

#ifndef ImageDecoder_h
#define ImageDecoder_h

#include "IntRect.h"
#include "ImageSource.h"
#include "PlatformString.h"
#include "SharedBuffer.h"
#include <wtf/Assertions.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

#if PLATFORM(SKIA)
#include "NativeImageSkia.h"
#elif PLATFORM(QT)
#include <QImage>
#endif

namespace WebCore {

    // The RGBA32Buffer object represents the decoded image data in RGBA32 format.  This buffer is what all
    // decoders write a single frame into.  Frames are then instantiated for drawing by being handed this buffer.
    class RGBA32Buffer {
    public:
        enum FrameStatus { FrameEmpty, FramePartial, FrameComplete };
        enum FrameDisposalMethod {
            // If you change the numeric values of these, make sure you audit all
            // users, as some users may cast raw values to/from these constants.
            DisposeNotSpecified,       // Leave frame in framebuffer
            DisposeKeep,               // Leave frame in framebuffer
            DisposeOverwriteBgcolor,   // Clear frame to transparent
            DisposeOverwritePrevious,  // Clear frame to previous framebuffer contents
        };
#if PLATFORM(SKIA) || PLATFORM(QT)
        typedef uint32_t PixelData;
#else
        typedef unsigned PixelData;
#endif

        RGBA32Buffer();

        // For backends which refcount their data, this constructor doesn't need
        // to create a new copy of the image data, only increase the ref count.
        //
        // This exists because ImageDecoder keeps a Vector<RGBA32Buffer>, and
        // Vector requires this constructor.
        RGBA32Buffer(const RGBA32Buffer& other)
        {
            operator=(other);
        }

        // Deletes the pixel data entirely; used by ImageDecoder to save memory
        // when we no longer need to display a frame and only need its metadata.
        void clear();

        // Zeroes the pixel data in the buffer, setting it to fully-transparent.
        void zeroFill();

        // Creates a new copy of the image data in |other|, so the two images
        // can be modified independently.
        void copyBitmapData(const RGBA32Buffer& other);

        // Copies the pixel data at [(startX, startY), (endX, startY)) to the
        // same X-coordinates on each subsequent row up to but not including
        // endY.
        void copyRowNTimes(int startX, int endX, int startY, int endY)
        {
            ASSERT(startX < width());
            ASSERT(endX <= width());
            ASSERT(startY < height());
            ASSERT(endY <= height());
            const int rowBytes = (endX - startX) * sizeof(PixelData);
            const PixelData* const startAddr = getAddr(startX, startY);
            for (int destY = startY + 1; destY < endY; ++destY)
                memcpy(getAddr(startX, destY), startAddr, rowBytes);
        }

        // Allocates space for the pixel data.  Must be called before any pixels
        // are written. Will return true on success, false if the memory
        // allocation fails.  Calling this multiple times is undefined and may
        // leak memory.
        bool setSize(int newWidth, int newHeight);

        // To be used by ImageSource::createFrameAtIndex().  Returns a pointer
        // to the underlying native image data.  This pointer will be owned by
        // the BitmapImage and freed in FrameData::clear().
        NativeImagePtr asNewNativeImage() const;

        bool hasAlpha() const;
        const IntRect& rect() const { return m_rect; }
        FrameStatus status() const { return m_status; }
        unsigned duration() const { return m_duration; }
        FrameDisposalMethod disposalMethod() const { return m_disposalMethod; }

        void setHasAlpha(bool alpha);
        void setRect(const IntRect& r) { m_rect = r; }
        void setStatus(FrameStatus status);
        void setDuration(unsigned duration) { m_duration = duration; }
        void setDisposalMethod(FrameDisposalMethod method) { m_disposalMethod = method; }

        inline void setRGBA(int x, int y, unsigned r, unsigned g, unsigned b, unsigned a)
        {
            setRGBA(getAddr(x, y), r, g, b, a);
        }

#if PLATFORM(QT)
        void setDecodedImage(const QImage& image);
        QImage decodedImage() const { return m_image; }
#endif

    private:
        RGBA32Buffer& operator=(const RGBA32Buffer& other);

        int width() const;
        int height() const;

        inline PixelData* getAddr(int x, int y)
        {
#if PLATFORM(SKIA)
            return m_bitmap.getAddr32(x, y);
#elif PLATFORM(QT)
            return reinterpret_cast<QRgb*>(m_image.scanLine(y)) + x;
#else
            return m_bytes.data() + (y * width()) + x;
#endif
        }

        inline void setRGBA(PixelData* dest, unsigned r, unsigned g, unsigned b, unsigned a)
        {
            // We store this data pre-multiplied.
            if (a == 0)
                *dest = 0;
            else {
                if (a < 255) {
                    float alphaPercent = a / 255.0f;
                    r = static_cast<unsigned>(r * alphaPercent);
                    g = static_cast<unsigned>(g * alphaPercent);
                    b = static_cast<unsigned>(b * alphaPercent);
                }
                *dest = (a << 24 | r << 16 | g << 8 | b);
            }
        }

#if PLATFORM(SKIA)
        NativeImageSkia m_bitmap;
#elif PLATFORM(QT)
        QImage m_image;
        bool m_hasAlpha;
        IntSize m_size;
#else
        Vector<PixelData> m_bytes;
        IntSize m_size;       // The size of the buffer.  This should be the
                              // same as ImageDecoder::m_size.
        bool m_hasAlpha;      // Whether or not any of the pixels in the buffer have transparency.
#endif
        IntRect m_rect;       // The rect of the original specified frame within the overall buffer.
                              // This will always just be the entire buffer except for GIF frames
                              // whose original rect was smaller than the overall image size.
        FrameStatus m_status; // Whether or not this frame is completely finished decoding.
        unsigned m_duration;  // The animation delay.
        FrameDisposalMethod m_disposalMethod;
                              // What to do with this frame's data when initializing the next frame.
    };

    // The ImageDecoder class represents a base class for specific image format decoders
    // (e.g., GIF, JPG, PNG, ICO) to derive from.  All decoders decode into RGBA32 format
    // and the base class manages the RGBA32 frame cache.
    class ImageDecoder {
    public:
        // ENABLE(IMAGE_DECODER_DOWN_SAMPLING) allows image decoders to write directly to
        // scaled output buffers by down sampling. Call setMaxNumPixels() to specify the
        // biggest size that decoded images can have. Image decoders will deflate those
        // images that are bigger than m_maxNumPixels. (Not supported by all image decoders yet)
        ImageDecoder()
            : m_failed(false)
            , m_sizeAvailable(false)
#if ENABLE(IMAGE_DECODER_DOWN_SAMPLING)
            , m_maxNumPixels(-1)
            , m_scaled(false)
#endif
        {
        }

        virtual ~ImageDecoder() {}

        // Factory function to create an ImageDecoder.  Ports that subclass
        // ImageDecoder can provide their own implementation of this to avoid
        // needing to write a dedicated setData() implementation.
        static ImageDecoder* create(const SharedBuffer& data);

        // The the filename extension usually associated with an undecoded image of this type.
        virtual String filenameExtension() const = 0;

        // All specific decoder plugins must do something with the data they are given.
        virtual void setData(SharedBuffer* data, bool allDataReceived) { m_data = data; }

        // Whether or not the size information has been decoded yet. This default
        // implementation just returns true if the size has been set and we have not
        // seen a failure. Decoders may want to override this to lazily decode
        // enough of the image to get the size.
        virtual bool isSizeAvailable()
        {
            return !m_failed && m_sizeAvailable; 
        }

        // Returns the size of the image.
        virtual IntSize size() const
        {
            // Requesting the size of an invalid bitmap is meaningless.
            ASSERT(!m_failed);
            return m_size;
        }

        // Returns the size of frame |index|.  This will only differ from size()
        // for formats where different frames are different sizes (namely ICO,
        // where each frame represents a different icon within the master file).
        // Notably, this does not return different sizes for different GIF
        // frames, since while these may be stored as smaller rectangles, during
        // decoding they are composited to create a full-size frame.
        virtual IntSize frameSizeAtIndex(size_t) const
        {
            return size();
        }

        // Called by the image decoders to set their decoded size, this also check
        // the size for validity. It will return true if the size was set, or false
        // if there is an error. On error, the m_failed flag will be set and the
        // caller should immediately stop decoding.
        virtual bool setSize(unsigned width, unsigned height)
        {
            if (isOverSize(width, height)) {
                m_failed = true;
                return false;
            }
            m_size = IntSize(width, height);
            m_sizeAvailable = true;
            return true;
        }

        // The total number of frames for the image.  Classes that support multiple frames
        // will scan the image data for the answer if they need to (without necessarily
        // decoding all of the individual frames).
        virtual size_t frameCount() { return 1; }

        // The number of repetitions to perform for an animation loop.
        virtual int repetitionCount() const { return cAnimationNone; }

        // Called to obtain the RGBA32Buffer full of decoded data for rendering.  The
        // decoder plugin will decode as much of the frame as it can before handing
        // back the buffer.
        virtual RGBA32Buffer* frameBufferAtIndex(size_t) = 0;

        // Whether or not the underlying image format even supports alpha transparency.
        virtual bool supportsAlpha() const { return true; }

        bool failed() const { return m_failed; }
        void setFailed() { m_failed = true; }

        // Wipe out frames in the frame buffer cache before |clearBeforeFrame|,
        // assuming this can be done without breaking decoding.  Different decoders
        // place different restrictions on what frames are safe to destroy, so this
        // is left to them to implement.
        // For convenience's sake, we provide a default (empty) implementation,
        // since in practice only GIFs will ever use this.
        virtual void clearFrameBufferCache(size_t clearBeforeFrame) { }

#if ENABLE(IMAGE_DECODER_DOWN_SAMPLING)
        void setMaxNumPixels(int m) { m_maxNumPixels = m; }
#endif

    protected:
#if ENABLE(IMAGE_DECODER_DOWN_SAMPLING)
        void prepareScaleDataIfNecessary();
        int upperBoundScaledX(int origX, int searchStart = 0);
        int lowerBoundScaledX(int origX, int searchStart = 0);
        int scaledY(int origY, int searchStart = 0);
#endif

        RefPtr<SharedBuffer> m_data; // The encoded data.
#if ENABLE(IMAGE_DECODER_DOWN_SAMPLING)
        int m_maxNumPixels;
        Vector<int> m_scaledColumns;
        Vector<int> m_scaledRows;
        bool m_scaled;
#endif
        Vector<RGBA32Buffer> m_frameBufferCache;
        bool m_failed;

    private:
        // Some code paths compute the size of the image as "width * height * 4"
        // and return it as a (signed) int.  Avoid overflow.
        static bool isOverSize(unsigned width, unsigned height)
        {
            // width * height must not exceed (2 ^ 29) - 1, so that we don't
            // overflow when we multiply by 4.
            unsigned long long total_size = static_cast<unsigned long long>(width)
                                          * static_cast<unsigned long long>(height);
            return total_size > ((1 << 29) - 1);
        }

        IntSize m_size;
        bool m_sizeAvailable;
    };

} // namespace WebCore

#endif
