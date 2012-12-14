/*
 * Copyright (C) 2011 Samsung Electronics
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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

#ifndef PageClientLegacyImpl_h
#define PageClientLegacyImpl_h

#include "PageClientBase.h"

namespace WebKit {

class PageClientLegacyImpl : public PageClientBase {
public:
    static PassOwnPtr<PageClientBase> create(EwkViewImpl* viewImpl)
    {
        return adoptPtr(new PageClientLegacyImpl(viewImpl));
    }

    virtual ~PageClientLegacyImpl() { }

    virtual void didCommitLoad();
    virtual void updateViewportSize();

    virtual WebCore::FloatRect convertToDeviceSpace(const WebCore::FloatRect&);
    virtual WebCore::FloatRect convertToUserSpace(const WebCore::FloatRect&);

private:
    explicit PageClientLegacyImpl(EwkViewImpl*);

    virtual void didChangeViewportProperties(const WebCore::ViewportAttributes&);
    virtual void didChangeContentsSize(const WebCore::IntSize&);
#if USE(TILED_BACKING_STORE)
    virtual void pageDidRequestScroll(const WebCore::IntPoint&);
    virtual void didRenderFrame(const WebCore::IntSize& contentsSize, const WebCore::IntRect& coveredRect);
    virtual void pageTransitionViewportReady();
#endif
};

} // namespace WebKit

#endif // PageClientLegacyImpl_h
