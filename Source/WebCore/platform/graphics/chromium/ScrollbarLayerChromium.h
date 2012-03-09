/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef ScrollbarLayerChromium_h
#define ScrollbarLayerChromium_h

#if USE(ACCELERATED_COMPOSITING)

#include "LayerChromium.h"
#include "ScrollTypes.h"

namespace WebCore {

class Scrollbar;

class ScrollbarLayerChromium : public LayerChromium {
public:
    virtual PassOwnPtr<CCLayerImpl> createCCLayerImpl();
    static PassRefPtr<ScrollbarLayerChromium> create(Scrollbar*, int scrollLayerId);

    virtual void pushPropertiesTo(CCLayerImpl*);

    int scrollLayerId() const { return m_scrollLayerId; }
    virtual ScrollbarLayerChromium* toScrollbarLayerChromium() { return this; }

protected:
    ScrollbarLayerChromium(Scrollbar*, int scrollLayerId);

private:
    RefPtr<Scrollbar> m_scrollbar;
    int m_scrollLayerId;

    ScrollbarOverlayStyle m_scrollbarOverlayStyle;
    bool m_isScrollableAreaActive;
    bool m_isScrollViewScrollbar;

    ScrollbarOrientation m_orientation;

    ScrollbarControlSize m_controlSize;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
