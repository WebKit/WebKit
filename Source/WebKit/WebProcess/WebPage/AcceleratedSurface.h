/*
 * Copyright (C) 2016 Igalia S.L.
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

#include <WebCore/IntSize.h>
#include <wtf/Noncopyable.h>

namespace WebKit {

class WebPage;

class AcceleratedSurface {
    WTF_MAKE_NONCOPYABLE(AcceleratedSurface); WTF_MAKE_FAST_ALLOCATED;
public:
    class Client {
    public:
        virtual void frameComplete() = 0;
    };

    static std::unique_ptr<AcceleratedSurface> create(WebPage&, Client&);
    virtual ~AcceleratedSurface() = default;

    virtual uint64_t window() const { ASSERT_NOT_REACHED(); return 0; }
    virtual uint64_t surfaceID() const { ASSERT_NOT_REACHED(); return 0; }
    virtual bool hostResize(const WebCore::IntSize&);
    virtual void clientResize(const WebCore::IntSize&) { };
    virtual bool shouldPaintMirrored() const { return false; }

    virtual void initialize() { }
    virtual void finalize() { }
    virtual void willRenderFrame() { }
    virtual void didRenderFrame() { }

protected:
    AcceleratedSurface(WebPage&, Client&);

    WebPage& m_webPage;
    Client& m_client;
    WebCore::IntSize m_size;
};

} // namespace WebKit
