/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#pragma once

#include "AcceleratedDrawingArea.h"
#include <WebCore/Region.h>

namespace WebCore {
class GraphicsContext;
}

namespace WebKit {

class ShareableBitmap;
class UpdateInfo;

class DrawingAreaImpl final : public AcceleratedDrawingArea {
public:
    DrawingAreaImpl(WebPage&, const WebPageCreationParameters&);
    virtual ~DrawingAreaImpl();

private:
    // DrawingArea
    void setNeedsDisplay() override;
    void setNeedsDisplayInRect(const WebCore::IntRect&) override;
    void scroll(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollDelta) override;
    void forceRepaint() override;

    void updatePreferences(const WebPreferencesStore&) override;

    void setRootCompositingLayer(WebCore::GraphicsLayer*) override;

    // IPC message handlers.
    void updateBackingStoreState(uint64_t backingStoreStateID, bool respondImmediately, float deviceScaleFactor, const WebCore::IntSize&, const WebCore::IntSize& scrollOffset) override;
    void didUpdate() override;

    // AcceleratedDrawingArea
    void suspendPainting() override;
    void sendDidUpdateBackingStoreState() override;
    void didUpdateBackingStoreState() override;

    void enterAcceleratedCompositingMode(WebCore::GraphicsLayer*) override;
    void exitAcceleratedCompositingMode() override;

    void scheduleDisplay();
    void displayTimerFired();
    void display();
    void display(UpdateInfo&);

    WebCore::Region m_dirtyRegion;
    WebCore::IntRect m_scrollRect;
    WebCore::IntSize m_scrollOffset;

    // Whether we're waiting for a DidUpdate message. Used for throttling paints so that the 
    // web process won't paint more frequent than the UI process can handle.
    bool m_isWaitingForDidUpdate { false };

    bool m_alwaysUseCompositing {false };
    bool m_forceRepaintAfterBackingStoreStateUpdate { false };

    RunLoop::Timer<DrawingAreaImpl> m_displayTimer;
};

} // namespace WebKit
