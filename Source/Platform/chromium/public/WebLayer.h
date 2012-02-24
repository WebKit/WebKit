/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef WebLayer_h
#define WebLayer_h

#include "WebColor.h"
#include "WebCommon.h"
#include "WebPrivatePtr.h"

class SkMatrix44;
namespace WebCore { class LayerChromium; }

namespace WebKit {
struct WebFloatPoint;
struct WebSize;

class WebLayer {
public:
    WEBKIT_EXPORT static WebLayer create();

    WebLayer() { }
    WebLayer(const WebLayer& layer) { assign(layer); }
    virtual ~WebLayer() { reset(); }
    WebLayer& operator=(const WebLayer& layer)
    {
        assign(layer);
        return *this;
    }
    bool isNull() { return m_private.isNull(); }
    WEBKIT_EXPORT void reset();
    WEBKIT_EXPORT void assign(const WebLayer&);
    WEBKIT_EXPORT bool equals(const WebLayer&) const;

    WEBKIT_EXPORT WebLayer rootLayer() const;
    WEBKIT_EXPORT WebLayer parent() const;
    WEBKIT_EXPORT void addChild(const WebLayer&);
    WEBKIT_EXPORT void insertChild(const WebLayer&, size_t index);
    WEBKIT_EXPORT void replaceChild(const WebLayer& reference, const WebLayer& newLayer);
    WEBKIT_EXPORT void removeFromParent();
    WEBKIT_EXPORT void removeAllChildren();

    WEBKIT_EXPORT void setAnchorPoint(const WebFloatPoint&);
    WEBKIT_EXPORT WebFloatPoint anchorPoint() const;

    WEBKIT_EXPORT void setAnchorPointZ(float);
    WEBKIT_EXPORT float anchorPointZ() const;

    WEBKIT_EXPORT void setBounds(const WebSize&);
    WEBKIT_EXPORT WebSize bounds() const;

    WEBKIT_EXPORT void setMasksToBounds(bool);
    WEBKIT_EXPORT bool masksToBounds() const;

    WEBKIT_EXPORT void setMaskLayer(const WebLayer&);
    WEBKIT_EXPORT WebLayer maskLayer() const;

    WEBKIT_EXPORT void setOpacity(float);
    WEBKIT_EXPORT float opacity() const;

    WEBKIT_EXPORT void setOpaque(bool);
    WEBKIT_EXPORT bool opaque() const;

    WEBKIT_EXPORT void setPosition(const WebFloatPoint&);
    WEBKIT_EXPORT WebFloatPoint position() const;

    WEBKIT_EXPORT void setSublayerTransform(const SkMatrix44&);
    WEBKIT_EXPORT SkMatrix44 sublayerTransform() const;

    WEBKIT_EXPORT void setTransform(const SkMatrix44&);
    WEBKIT_EXPORT SkMatrix44 transform() const;

    WEBKIT_EXPORT void setDebugBorderColor(const WebColor&);
    WEBKIT_EXPORT void setDebugBorderWidth(float);

    template<typename T> T to()
    {
        T res;
        res.WebLayer::assign(*this);
        return res;
    }

    template<typename T> const T toConst() const
    {
        T res;
        res.WebLayer::assign(*this);
        return res;
    }

#if WEBKIT_IMPLEMENTATION
    WebLayer(const WTF::PassRefPtr<WebCore::LayerChromium>&);
    WebLayer& operator=(const WTF::PassRefPtr<WebCore::LayerChromium>&);
    operator WTF::PassRefPtr<WebCore::LayerChromium>() const;
    template<typename T> T* unwrap()
    {
        return static_cast<T*>(m_private.get());
    }

    template<typename T> const T* constUnwrap() const
    {
        return static_cast<const T*>(m_private.get());
    }
#endif

protected:
    WebPrivatePtr<WebCore::LayerChromium> m_private;
};

inline bool operator==(const WebLayer& a, const WebLayer& b)
{
    return a.equals(b);
}

inline bool operator!=(const WebLayer& a, const WebLayer& b)
{
    return !(a == b);
}

} // namespace WebKit

#endif // WebLayer_h
