/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebPluginContainerImpl_h
#define WebPluginContainerImpl_h

// FIXME: This relative path is a temporary hack to support using this
// header from webkit/glue.
#include "../public/WebPluginContainer.h"

#include "Widget.h"
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

struct NPObject;

namespace WebCore {
class HTMLPlugInElement;
class IntRect;
class KeyboardEvent;
class MouseEvent;
class ResourceError;
class ResourceResponse;
}

namespace WebKit {

class WebPlugin;
class WebPluginLoadObserver;

class WebPluginContainerImpl : public WebCore::Widget, public WebPluginContainer {
public:
    static PassRefPtr<WebPluginContainerImpl> create(WebCore::HTMLPlugInElement* element, WebPlugin* webPlugin)
    {
        return adoptRef(new WebPluginContainerImpl(element, webPlugin));
    }

    // Widget methods
    virtual void setFrameRect(const WebCore::IntRect&);
    virtual void paint(WebCore::GraphicsContext*, const WebCore::IntRect&);
    virtual void invalidateRect(const WebCore::IntRect&);
    virtual void setFocus();
    virtual void show();
    virtual void hide();
    virtual void handleEvent(WebCore::Event*);
    virtual void frameRectsChanged();
    virtual void setParentVisible(bool);
    virtual void setParent(WebCore::ScrollView*);

    // WebPluginContainer methods
    virtual void invalidate();
    virtual void invalidateRect(const WebRect&);
    virtual void reportGeometry();
    virtual void clearScriptObjects();
    virtual NPObject* scriptableObjectForElement();
    virtual WebString executeScriptURL(const WebURL&, bool popupsAllowed);
    virtual void loadFrameRequest(const WebURLRequest&, const WebString& target, bool notifyNeeded, void* notifyData);

    // Resource load events for the plugin's source data:
    void didReceiveResponse(const WebCore::ResourceResponse&);
    void didReceiveData(const char *data, int dataLength);
    void didFinishLoading();
    void didFailLoading(const WebCore::ResourceError&);

    NPObject* scriptableObject();

    // This cannot be null.
    WebPlugin* plugin() { return m_webPlugin; }

    void willDestroyPluginLoadObserver(WebPluginLoadObserver*);

private:
    WebPluginContainerImpl(WebCore::HTMLPlugInElement* element, WebPlugin* webPlugin)
        : m_element(element)
        , m_webPlugin(webPlugin) { }
    ~WebPluginContainerImpl();

    void handleMouseEvent(WebCore::MouseEvent*);
    void handleKeyboardEvent(WebCore::KeyboardEvent*);

    void calculateGeometry(const WebCore::IntRect& frameRect,
                           WebCore::IntRect& windowRect,
                           WebCore::IntRect& clipRect,
                           Vector<WebCore::IntRect>& cutOutRects);
    WebCore::IntRect windowClipRect() const;
    void windowCutOutRects(const WebCore::IntRect& frameRect,
                           Vector<WebCore::IntRect>& cutOutRects);

    WebCore::HTMLPlugInElement* m_element;
    WebPlugin* m_webPlugin;
    Vector<WebPluginLoadObserver*> m_pluginLoadObservers;
};

} // namespace WebKit

#endif
