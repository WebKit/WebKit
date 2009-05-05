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

    // The RGBA32Buffer object represents the decoded image data in RGBA32 format.
    // This buffer is what all decoders write a single frame into.  Frames are then
    // instantiated for drawing by being handed this buffer.
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
        }

        // This constructor doesn't create a new copy of the image data, it only
        // increases the ref count of the existing bitmap.
        RGBA32Buffer(const RGBA32Buffer& other)
        {
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

            m_bitmap = other.m_bitmap;
            // Keep the pixels locked since we will be writing directly into the
            // bitmap throughout this object's lifetime.
            m_bitmap.lockPixels();
            setRect(other.rect());
            setStatus(other.status());
            setDuration(other.duration());
            setDisposalMethod(other.disposalMethod());
            return *this;
        }

        void clear()
        {
            m_bitmap.reset();
            m_status = FrameEmpty;
            // NOTE: Do not reset other members here; clearFrameBufferCache()
            // calls this to free the bitmap data, but other functions like
            // initFrameBuffer() and frameComplete() may still need to read
            // other metadata out of this frame later.
        }

        // This function creates a new copy of the image data in |other|, so the
        // two images can be modified independently.
        void copyBitmapData(const RGBA32Buffer& other)
        {
            if (this == &other)
                return;

            m_bitmap.reset();
            const NativeImageSkia& otherBitmap = other.bitmap();
            otherBitmap.copyTo(&m_bitmap, otherBitmap.config());
        }

        NativeImageSkia& bitmap() { return m_bitmap; }
        const NativeImageSkia& bitmap() const { return m_bitmap; }

        // Must be called before any pixels are written. Will return true on
        // success, false if the memory allocation fails.
        bool setSize(int width, int height)
        {
            // This function should only be called once, it will leak memory
            // otherwise.
            ASSERT(m_bitmap.width() == 0 && m_bitmap.height() == 0);
            m_bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);
            if (!m_bitmap.allocPixels())
                return false;  // Allocation failure, maybe the bitmap was too big.

            // Clear the image.
            m_bitmap.eraseARGB(0, 0, 0, 0);

            return true;
        }

        int width() const { return m_bitmap.width(); }
        int height() const { return m_bitmap.height(); }

        const IntRect& rect() const { return m_rect; }
        FrameStatus status() const { return m_status; }
        unsigned duration() const { return m_duration; }
        FrameDisposalMethod disposalMethod() const { return m_disposalMethod; }
        bool hasAlpha() const { return !m_bitmap.isOpaque(); }

        void setRect(const IntRect& r) { m_rect = r; }
        void setStatus(FrameStatus s)
        {
            if (s == FrameComplete)
                m_bitmap.setDataComplete();  // Tell the bitmap it's done.
            m_status = s;
        }
        void setDuration(unsigned duration) { m_duration = duration; }
        void setDisposalMethod(FrameDisposalMethod method) { m_disposalMethod = method; }
        void setHasAlpha(bool alpha) { m_bitmap.setIsOpaque(!alpha); }

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
            setRGBA(m_bitmap.getAddr32(x, y), r, g, b, a);
        }

    private:
        NativeImageSkia m_bitmap;
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
