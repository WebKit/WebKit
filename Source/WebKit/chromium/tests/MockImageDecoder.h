/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MockImageDecoder_h

#include "ImageDecoder.h"

namespace WebCore {

class MockImageDecoderClient {
public:
    virtual void decoderBeingDestroyed() = 0;
    virtual void frameBufferRequested() = 0;
    virtual void frameBuffersLocked() { }
    virtual void frameBuffersUnlocked() { }
    virtual ImageFrame::FrameStatus frameStatus() = 0;
};

class MockImageDecoder : public ImageDecoder {
public:
    static PassOwnPtr<MockImageDecoder> create(MockImageDecoderClient* client) { return adoptPtr(new MockImageDecoder(client)); }

    MockImageDecoder(MockImageDecoderClient* client)
        : ImageDecoder(ImageSource::AlphaPremultiplied, ImageSource::GammaAndColorProfileApplied)
        , m_frameBufferRequestCount(0)
        , m_client(client)
    { }

    ~MockImageDecoder()
    {
        m_client->decoderBeingDestroyed();
    }

    virtual bool setSize(unsigned width, unsigned height)
    {
        ImageDecoder::setSize(width, height);
        m_frameBufferCache.resize(1);
        m_frameBufferCache[0].setSize(width, height);
        return true;
    }

    virtual void setFrameHasAlpha(bool hasAlpha) { m_frameBufferCache[0].setHasAlpha(hasAlpha); }

    virtual String filenameExtension() const
    {
        return "mock";
    }

    virtual ImageFrame* frameBufferAtIndex(size_t)
    {
        ++m_frameBufferRequestCount;
        m_client->frameBufferRequested();

        m_frameBufferCache[0].setStatus(m_client->frameStatus());
        return &m_frameBufferCache[0];
    }

    virtual bool lockFrameBuffers()
    {
        m_client->frameBuffersLocked();
        return true;
    }
    virtual void unlockFrameBuffers() { m_client->frameBuffersUnlocked(); }

    int frameBufferRequestCount() const { return m_frameBufferRequestCount; }

private:
    int m_frameBufferRequestCount;
    MockImageDecoderClient* m_client;
};

} // namespace WebCore

#endif // MockImageDecoder_h
