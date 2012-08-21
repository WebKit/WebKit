/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebScrollableLayer_h
#define WebScrollableLayer_h

#include "WebCommon.h"
#include "WebLayer.h"
#include "WebPoint.h"
#include "WebRect.h"
#include "WebVector.h"

namespace WebKit {

class WebScrollableLayer : public WebLayer {
public:
    WebScrollableLayer() { }
    WebScrollableLayer(const WebScrollableLayer& layer) : WebLayer(layer) { }
    virtual ~WebScrollableLayer() { }
    WebScrollableLayer& operator=(const WebScrollableLayer& layer)
    {
        WebLayer::assign(layer);
        return *this;
    }

    WEBKIT_EXPORT void setScrollPosition(WebPoint);
    WEBKIT_EXPORT void setScrollable(bool);
    WEBKIT_EXPORT void setHaveWheelEventHandlers(bool);
    WEBKIT_EXPORT void setShouldScrollOnMainThread(bool);
    WEBKIT_EXPORT void setNonFastScrollableRegion(const WebVector<WebRect>&);
    WEBKIT_EXPORT void setIsContainerForFixedPositionLayers(bool);
    WEBKIT_EXPORT void setFixedToContainerLayer(bool);


#if WEBKIT_IMPLEMENTATION
    WebScrollableLayer(const WTF::PassRefPtr<WebCore::LayerChromium>& layer) : WebLayer(layer) { }
#endif
};

} // namespace WebKit

#endif // WebScrollableLayer_h
