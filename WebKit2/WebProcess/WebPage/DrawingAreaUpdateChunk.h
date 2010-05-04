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

#ifndef DrawingAreaUpdateChunk_h
#define DrawingAreaUpdateChunk_h

#include "DrawingArea.h"
#include "RunLoop.h"
#include <WebCore/IntPoint.h>

namespace WebKit {

class UpdateChunk;

class DrawingAreaUpdateChunk : public DrawingArea {
public:
    DrawingAreaUpdateChunk(WebPage*);
    virtual ~DrawingAreaUpdateChunk();

    virtual void invalidateWindow(const WebCore::IntRect& rect, bool immediate);
    virtual void invalidateContentsAndWindow(const WebCore::IntRect& rect, bool immediate);
    virtual void invalidateContentsForSlowScroll(const WebCore::IntRect& rect, bool immediate);
    virtual void scroll(const WebCore::IntSize& scrollDelta, const WebCore::IntRect& rectToScroll, const WebCore::IntRect& clipRect);
    virtual void setNeedsDisplay(const WebCore::IntRect&);
    virtual void display();

    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder&);

private:
    void scheduleDisplay();
    
    // CoreIPC message handlers.
    void setSize(const WebCore::IntSize& viewSize);
    void suspendPainting();
    void resumePainting();
    void didUpdate();

    // Platform overrides
    void paintIntoUpdateChunk(UpdateChunk*);

    WebCore::IntRect m_dirtyRect;
    bool m_isWaitingForUpdate;
    bool m_shouldPaint;
    RunLoop::Timer<DrawingAreaUpdateChunk> m_displayTimer;
};

} // namespace WebKit

#endif // DrawingAreaUpdateChunk_h
