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

#include "ArgumentEncoder.h"

namespace CoreIPC {
    class ArgumentDecoder;
    class Connection;
    class MessageID;
}

namespace WebCore {
    class IntSize;
    class IntRect;
}

namespace WebKit {

#if PLATFORM(MAC)
typedef CGContextRef PlatformDrawingContext;
#elif PLATFORM(WIN)
typedef HDC PlatformDrawingContext;
#endif

class DrawingAreaProxy {
public:
    enum Type {
        DrawingAreaUpdateChunkType
    };

    virtual ~DrawingAreaProxy();

    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder&) = 0;

    virtual void paint(const WebCore::IntRect&, PlatformDrawingContext) = 0;
    virtual void setSize(const WebCore::IntSize&) = 0;

    virtual void setPageIsVisible(bool isVisible) = 0;
    
    // The DrawingAreaProxy should never be decoded itself. Instead, the DrawingArea should be decoded.
    virtual void encode(CoreIPC::ArgumentEncoder& encoder) const
    {
        encoder.encode(static_cast<uint32_t>(m_type));
    }

protected:
    DrawingAreaProxy(Type);

    Type m_type;
};
    
} // namespace WebKit

#endif // DrawingAreaProxy_h
