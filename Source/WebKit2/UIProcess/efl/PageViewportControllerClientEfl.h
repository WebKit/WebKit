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

#include "EwkViewImpl.h"
#include "PageClientBase.h"
#include "PageViewportControllerClient.h"
#include <wtf/PassOwnPtr.h>

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
    WebCore::FloatPoint contentPosition() const { return m_contentPosition; }

    void updateViewportSize();
    void setRendererActive(bool);

    virtual void setViewportPosition(const WebCore::FloatPoint& contentsPoint);
    virtual void setPageScaleFactor(float);

    virtual void didResumeContent();
    virtual void didChangeContentsSize(const WebCore::IntSize&);
    virtual void didChangeVisibleContents();
    virtual void didChangeViewportAttributes();

    virtual void setController(PageViewportController*);

private:
    explicit PageViewportControllerClientEfl(EwkViewImpl*);

    EwkViewImpl* m_viewImpl;
    WebCore::IntSize m_viewportSize;
    WebCore::FloatPoint m_contentPosition;
    PageViewportController* m_controller;
};

} // namespace WebKit

#endif

#endif // PageViewportControllerClientEfl_h
