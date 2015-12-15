/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef MockPageOverlayClient_h
#define MockPageOverlayClient_h

#include "MockPageOverlay.h"
#include "PageOverlay.h"
#include <wtf/HashSet.h>

namespace WebCore {

class MainFrame;

class MockPageOverlayClient final : public PageOverlay::Client {
    friend class NeverDestroyed<MockPageOverlayClient>;
public:
    static MockPageOverlayClient& singleton();

    explicit MockPageOverlayClient();

    Ref<MockPageOverlay> installOverlay(MainFrame&, PageOverlay::OverlayType);
    void uninstallAllOverlays();

    String layerTreeAsText(MainFrame&);

    virtual ~MockPageOverlayClient() { }

private:
    virtual void pageOverlayDestroyed(PageOverlay&) override;
    virtual void willMoveToPage(PageOverlay&, Page*) override;
    virtual void didMoveToPage(PageOverlay&, Page*) override;
    virtual void drawRect(PageOverlay&, GraphicsContext&, const IntRect& dirtyRect) override;
    virtual bool mouseEvent(PageOverlay&, const PlatformMouseEvent&) override;
    virtual void didScrollFrame(PageOverlay&, Frame&) override;

    virtual bool copyAccessibilityAttributeStringValueForPoint(PageOverlay&, String /* attribute */, FloatPoint, String&) override;
    virtual bool copyAccessibilityAttributeBoolValueForPoint(PageOverlay&, String /* attribute */, FloatPoint, bool&) override;
    virtual Vector<String> copyAccessibilityAttributeNames(PageOverlay&, bool /* parameterizedNames */) override;

    HashSet<MockPageOverlay*> m_overlays;
};

}

#endif // MockPageOverlayClient_h
