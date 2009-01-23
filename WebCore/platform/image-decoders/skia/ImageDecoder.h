/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2008, 2009 Google, Inc.
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
#include "NativeImageSkia.h"
#include "PlatformString.h"
#include "SharedBuffer.h"
#include <wtf/Assertions.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

#include "SkBitmap.h"

namespace WebCore {

    class RefCountedNativeImageSkia : public RefCounted<RefCountedNativeImageSkia> {
    public:
        static PassRefPtr<RefCountedNativeImageSkia> create()
        {
            return adoptRef(new RefCountedNativeImageSkia);
        }

        const NativeImageSkia& bitmap() const { return m_bitmap; }
        NativeImageSkia& bitmap() { return m_bitmap; }

    private:
        RefCountedNativeImageSkia() {}
        NativeImageSkia m_bitmap;
    };

    // The RGBA32Buffer object represents the decoded image data in RGBA32 format.
    // This buffer is what all decoders write a single frame into.  Frames are then
    // instantiated for drawing by being handed this buffer.
    //
    // The image data of an RGBA32Buffer is kept in an SkBitmapRef, a refcounting
    // container for an SkBitmap.  In all normal cases, the refcount should be
    // exactly one.  The exception happens when resizing the vector<RGBA32Buffer> in
    // ImageDecoder::m_frameBufferCache, which copies all the buffers to the new
    // vector, thus transiently incrementing the refcount to two.
    //
    // The choice to use an SkBitmapRef instead of an SkBitmap is not because of
    // performance concerns -- SkBitmap refcounts its internal data anyway.  Rather,
    // we need the aforementioned vector resize to preserve the exact underlying
    // SkBitmap object, since we may have already given its address to
    // BitmapImage::m_frames.  Using an SkBitmap would mean that after ImageDecoder
    // resizes its vector, BitmapImage would be holding a pointer to garbage.
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

        RGBA32Buffer()
            : m_status(FrameEmpty)
            , m_duration(0)
            , m_disposalMethod(DisposeNotSpecified)
        {
            m_bitmapRef = RefCountedNativeImageSkia::create();
        }

        // This constructor doesn't create a new copy of the image data, it only
        // increases the ref count of the existing bitmap.
        RGBA32Buffer(const RGBA32Buffer& other)
        {
            m_bitmapRef = RefCountedNativeImageSkia::create();
            operator=(other);
        }

        ~RGBA32Buffer()
        {
        }

        // Initialize with another buffer.  This function doesn't create a new copy
        // of the image data, it only increases the refcount of the existing bitmap.
        //
        // Normal callers should not generally be using this function.  If you want
        // to create a copy on which you can modify the image data independently,
        // use copyBitmapData() instead.
        RGBA32Buffer& operator=(const RGBA32Buffer& other)
        {
            if (this == &other)
                return *this;

            m_bitmapRef = other.m_bitmapRef;
            setRect(other.rect());
            setStatus(other.status());
            setDuration(other.duration());
            setDisposalMethod(other.disposalMethod());
            setHasAlpha(other.hasAlpha());
            return *this;
        }

        void clear()
        {
            m_bitmapRef = RefCountedNativeImageSkia::create();
            m_rect = IntRect();
            m_status = FrameEmpty;
            m_duration = 0;
            m_disposalMethod = DisposeNotSpecified;
        }

        // This function creates a new copy of the image data in |other|, so the
        // two images can be modified independently.
        void copyBitmapData(const RGBA32Buffer& other)
        {
            if (this == &other)
                return;

            m_bitmapRef = RefCountedNativeImageSkia::create();
            SkBitmap& bmp = bitmap();
            const SkBitmap& otherBmp = other.bitmap();
            bmp.setConfig(SkBitmap::kARGB_8888_Config, other.width(),
                          other.height(), otherBmp.rowBytes());
            bmp.allocPixels();
            if (width() > 0 && height() > 0) {
                memcpy(bmp.getAddr32(0, 0),
                       otherBmp.getAddr32(0, 0),
                       otherBmp.rowBytes() * height());
            }
        }

        NativeImageSkia& bitmap() { return m_bitmapRef->bitmap(); }
        const NativeImageSkia& bitmap() const { return m_bitmapRef->bitmap(); }

        // Must be called before any pixels are written. Will return true on
        // success, false if the memory allocation fails.
        bool setSize(int width, int height)
        {
            // This function should only be called once, it will leak memory
            // otherwise.
            SkBitmap& bmp = bitmap();
            ASSERT(bmp.width() == 0 && bmp.height() == 0);
            bmp.setConfig(SkBitmap::kARGB_8888_Config, width, height);
            if (!bmp.allocPixels())
                return false;  // Allocation failure, maybe the bitmap was too big.

            // Clear the image.
            bmp.eraseARGB(0, 0, 0, 0);

            return true;
        }

        int width() const { return bitmap().width(); }
        int height() const { return bitmap().height(); }

        const IntRect& rect() const { return m_rect; }
        FrameStatus status() const { return m_status; }
        unsigned duration() const { return m_duration; }
        FrameDisposalMethod disposalMethod() const { return m_disposalMethod; }
        bool hasAlpha() const { return !bitmap().isOpaque(); }

        void setRect(const IntRect& r) { m_rect = r; }
        void setStatus(FrameStatus s)
        {
            if (s == FrameComplete)
                m_bitmapRef->bitmap().setDataComplete();  // Tell the bitmap it's done.
            m_status = s;
        }
        void setDuration(unsigned duration) { m_duration = duration; }
        void setDisposalMethod(FrameDisposalMethod method) { m_disposalMethod = method; }
        void setHasAlpha(bool alpha) { bitmap().setIsOpaque(!alpha); }

        static void setRGBA(uint32_t* dest, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
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

        void setRGBA(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
        {
            setRGBA(bitmap().getAddr32(x, y), r, g, b, a);
        }

    private:
        RefPtr<RefCountedNativeImageSkia> m_bitmapRef;
        IntRect m_rect;    // The rect of the original specified frame within the overall buffer.
                           // This will always just be the entire buffer except for GIF frames
                           // whose original rect was smaller than the overall image size.
        FrameStatus m_status; // Whether or not this frame is completely finished decoding.
        unsigned m_duration; // The animation delay.
        FrameDisposalMethod m_disposalMethod; // What to do with this frame's data when initializing the next frame.
    };

    // The ImageDecoder class represents a base class for specific image format decoders
    // (e.g., GIF, JPG, PNG, ICO) to derive from.  All decoders decode into RGBA32 format
    // and the base class manages the RGBA32 frame cache.
    class ImageDecoder {
    public:
        ImageDecoder() : m_failed(false), m_sizeAvailable(false)  {}
        virtual ~ImageDecoder() {}

        // The the filename extension usually associated with an undecoded image of this type.
        virtual String filenameExtension() const = 0;

        // All specific decoder plugins must do something with the data they are given.
        virtual void setData(SharedBuffer* data, bool allDataReceived) { m_data = data; }

        // Whether or not the size information has been decoded yet. This default
        // implementation just returns true if the size has been set and we have not
        // seen a failure. Decoders may want to override this to lazily decode
        // enough of the image to get the size.
        virtual bool isSizeAvailable() const
        {
            return !m_failed && m_sizeAvailable; 
        }

        // Requests the size.
        virtual IntSize size() const
        {
            // Requesting the size of an invalid bitmap is meaningless.
            ASSERT(!m_failed);
            return m_size;
        }

        // The total number of frames for the image.  Classes that support multiple frames
        // will scan the image data for the answer if they need to (without necessarily
        // decoding all of the individual frames).
        virtual int frameCount() { return 1; }

        // The number of repetitions to perform for an animation loop.
        virtual int repetitionCount() const { return cAnimationNone; }

        // Called to obtain the RGBA32Buffer full of decoded data for rendering.  The
        // decoder plugin will decode as much of the frame as it can before handing
        // back the buffer.
        virtual RGBA32Buffer* frameBufferAtIndex(size_t index) = 0;

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

    protected:
        // Called by the image decoders to set their decoded size, this also check
        // the size for validity. It will return true if the size was set, or false
        // if there is an error. On error, the m_failed flag will be set and the
        // caller should immediately stop decoding.
        bool setSize(unsigned width, unsigned height)
        {
            if (isOverSize(width, height)) {
                m_failed = true;
                return false;
            }
            m_size = IntSize(width, height);
            m_sizeAvailable = true;
            return true;
        }

        RefPtr<SharedBuffer> m_data; // The encoded data.
        Vector<RGBA32Buffer> m_frameBufferCache;
        mutable bool m_failed;

    private:
        // This function allows us to make sure the image is not too large. Very
        // large images, even if the allocation succeeds, can take a very long time
        // to process, giving the appearance of a DoS.
        //
        // WebKit also seems to like to ask for the size before data is available
        // and in some cases when the failed flag is set. Some of these code paths
        // such as BitmapImage::resetAnimation then compute the size of the image
        // based on the width and height. Because of this, our total computed image
        // byte size must never overflow an int.
        static bool isOverSize(unsigned width, unsigned height)
        {
            unsigned long long total_size = static_cast<unsigned long long>(width)
                                          * static_cast<unsigned long long>(height);
            if (total_size > 32 * 1024 * 1024)  // 32M = 128MB memory total (32 bpp).
                return true;
            return false;
        }

        IntSize m_size;
        bool m_sizeAvailable;
    };

} // namespace WebCore

#endif
