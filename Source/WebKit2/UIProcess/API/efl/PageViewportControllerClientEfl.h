/*
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
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

#ifndef PageViewportControllerClientEfl_h
#define PageViewportControllerClientEfl_h

#if USE(TILED_BACKING_STORE)

#include "PageClientImpl.h"
#include "PageViewportControllerClient.h"
#include <wtf/PassOwnPtr.h>

class EwkViewImpl;

namespace WebKit {

class PageViewportControllerClientEfl : public PageViewportControllerClient {
public:
    static PassOwnPtr<PageViewportControllerClientEfl> create(EwkViewImpl* viewImpl)
    {
        return adoptPtr(new PageViewportControllerClientEfl(viewImpl));
    }
    ~PageViewportControllerClientEfl();

    DrawingAreaProxy* drawingArea() const;
    WebCore::IntSize viewSize() { return m_viewportSize; }
    float scaleFactor() const { return m_scaleFactor; }
    WebCore::IntPoint scrollPosition() { return m_scrollPosition; }

    void display(const WebCore::IntRect& rect, const WebCore::IntPoint& viewPosition);
    void updateViewportSize(const WebCore::IntSize& viewportSize);
    void setVisibleContentsRect(const WebCore::IntPoint&, float, const WebCore::FloatPoint&);
    void setRendererActive(bool);

    virtual void setViewportPosition(const WebCore::FloatPoint& contentsPoint);
    virtual void setContentsScale(float, bool treatAsInitialValue);

    virtual void didResumeContent();
    virtual void didChangeContentsSize(const WebCore::IntSize&);
    virtual void didChangeVisibleContents();
    virtual void didChangeViewportAttributes();

    virtual void setController(PageViewportController*);

private:
    explicit PageViewportControllerClientEfl(EwkViewImpl*);

    EwkViewImpl* m_viewImpl;
    WebCore::IntSize m_contentsSize;
    WebCore::IntSize m_viewportSize;
    WebCore::IntPoint m_scrollPosition;
    float m_scaleFactor;
    PageViewportController* m_pageViewportController;
};

} // namespace WebKit

#endif

#endif // PageViewportControllerClientEfl_h
