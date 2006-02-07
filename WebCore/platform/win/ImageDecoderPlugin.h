/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef IMAGE_DECODER_PLUGIN_H_
#define IMAGE_DECODER_PLUGIN_H_

#include "config.h"
#include "IntSize.h"
#include "ImageDecoder.h"
#include <kxmlcore/Vector.h>
#include "Array.h"

namespace WebCore {

typedef Array<char> ByteArray;
typedef Array<unsigned> RGBA32Array;

// The RGBA32Buffer object represents the decoded image data in RGBA32 format.  This buffer is what all
// decoders write a single frame into.  Frames are then instantiated for drawing by being handed this buffer.
class RGBA32Buffer
{
public:
    enum FrameStatus { FrameEmpty, FramePartial, FrameComplete };

    RGBA32Buffer() : m_status(FrameEmpty), m_duration(0), m_includeInNextFrame(false)
    {} 

    void setStatus(FrameStatus s) { m_status = s; }
    void setDuration(unsigned duration) { m_duration = duration; }
    void setIncludeInNextFrame(bool n) { m_includeInNextFrame = n; }

    RGBA32Array& bytes() { return m_bytes; }
    FrameStatus status() const { return m_status; }
    unsigned duration() const { return m_duration; }
    bool includeInNextFrame() const { return m_includeInNextFrame; }

    void setRGBA(unsigned pos, unsigned r, unsigned b, unsigned g, unsigned a)
    {
        unsigned rgba = (a << 24 | r << 16 | g << 8 | b);
        m_bytes[pos] = rgba;
    }

private:
    RGBA32Array m_bytes;
    FrameStatus m_status; // Whether or not this frame is completely finished decoding.
    unsigned m_duration; // The animation delay.
    bool m_includeInNextFrame; // Whether or not the next buffer should be initially populated with our data.
};

// The ImageDecoderPlugin class represents a base class for specific image format decoders
// (e.g., GIF, JPG, PNG, ICO) to derive from.  All decoders decode into RGBA32 format
// and the base class manages the RGBA32 frame cache.
class ImageDecoderPlugin
{
public:
    ImageDecoderPlugin() {};
    virtual ~ImageDecoderPlugin() {};

    // All specific decoder plugins must do something with the data they are given.
    virtual void setData(const ByteArray& data, bool allDataReceived) { m_data = data; }

    // Whether or not the size information has been decoded yet.
    virtual bool isSizeAvailable() const { return false; }

    // Requests the size.
    virtual IntSize size() const { return IntSize(); }

    // The total number of frames for the image.  Classes that support multiple frames
    // will scan the image data for the answer if they need to (without necessarily
    // decoding all of the individual frames).
    virtual int frameCount() { return 1; }

    // The number of repetitions to perform for an animation loop.
    virtual int repetitionCount() const { return cAnimationNone; }

    // Called to obtain the RGBA32Buffer full of decoded data for rendering.  The
    // decoder plugin will decode as much of the frame as it can before handing
    // back the buffer.
    virtual RGBA32Buffer frameBufferAtIndex(size_t index) { return RGBA32Buffer(); }

    // This method will examine the data and instantiate an instance of the appropriate decoder plugin.
    // If insufficient bytes are available to determine the image type, no decoder plugin will be
    // made.
    static ImageDecoder* createDecoderPlugin(const ByteArray& data);

protected:
    ByteArray m_data; // The encoded data.
    Vector<RGBA32Buffer> m_frameBufferCache;
};

}

#endif
