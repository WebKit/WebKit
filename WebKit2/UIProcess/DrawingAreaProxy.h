/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef DrawingAreaProxy_h
#define DrawingAreaProxy_h

#include "DrawingAreaInfo.h"
#include <WebCore/IntSize.h>

#if PLATFORM(QT)
class QPainter;
#endif

namespace WebKit {

class WebPageProxy;
struct UpdateInfo;
    
#if PLATFORM(MAC)
typedef CGContextRef PlatformDrawingContext;
#elif PLATFORM(WIN)
typedef HDC PlatformDrawingContext;
#elif PLATFORM(QT)
typedef QPainter* PlatformDrawingContext;
#endif

class DrawingAreaProxy {
    WTF_MAKE_NONCOPYABLE(DrawingAreaProxy);

public:
    static DrawingAreaInfo::Identifier nextIdentifier();

    virtual ~DrawingAreaProxy();

#ifdef __APPLE__
    void didReceiveDrawingAreaProxyMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
#endif

    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*) = 0;
    virtual void didReceiveSyncMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, CoreIPC::ArgumentEncoder*) { ASSERT_NOT_REACHED(); }

    // Returns true if painting was successful, false otherwise.
    virtual bool paint(const WebCore::IntRect&, PlatformDrawingContext) = 0;

    virtual void sizeDidChange() = 0;
    virtual void setPageIsVisible(bool isVisible) = 0;
    
#if USE(ACCELERATED_COMPOSITING)
    virtual void attachCompositingContext(uint32_t contextID) = 0;
    virtual void detachCompositingContext() = 0;
#endif

    const DrawingAreaInfo& info() const { return m_info; }

    const WebCore::IntSize& size() const { return m_size; }
    void setSize(const WebCore::IntSize&);

protected:
    explicit DrawingAreaProxy(DrawingAreaInfo::Type, WebPageProxy*);

    DrawingAreaInfo m_info;
    WebPageProxy* m_webPageProxy;

    WebCore::IntSize m_size;

private:
    // CoreIPC message handlers.
    // FIXME: These should be pure virtual.
    virtual void update(const UpdateInfo&) { }
    virtual void didSetSize() { }
};

} // namespace WebKit

#endif // DrawingAreaProxy_h
