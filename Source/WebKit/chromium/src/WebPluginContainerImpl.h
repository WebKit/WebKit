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

#include "PluginViewBase.h"
#include "WebPluginContainer.h"
#include "Widget.h"

#include <public/WebExternalTextureLayer.h>
#include <public/WebIOSurfaceLayer.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

struct NPObject;

namespace WebCore {
class HTMLPlugInElement;
class IntRect;
class KeyboardEvent;
class LayerChromium;
class MouseEvent;
class ResourceError;
class ResourceResponse;
class WheelEvent;

#if ENABLE(GESTURE_EVENTS)
class PlatformGestureEvent;
#endif
}

namespace WebKit {

struct WebPrintParams;

class ScrollbarGroup;
class WebPlugin;
class WebPluginLoadObserver;

class WebPluginContainerImpl : public WebCore::PluginViewBase, public WebPluginContainer {
public:
    static PassRefPtr<WebPluginContainerImpl> create(WebCore::HTMLPlugInElement* element, WebPlugin* webPlugin)
    {
        return adoptRef(new WebPluginContainerImpl(element, webPlugin));
    }

    // PluginViewBase methods
    virtual bool getFormValue(String&);

    // Widget methods
    virtual void setFrameRect(const WebCore::IntRect&);
    virtual void paint(WebCore::GraphicsContext*, const WebCore::IntRect&);
    virtual void invalidateRect(const WebCore::IntRect&);
    virtual void setFocus(bool);
    virtual void show();
    virtual void hide();
    virtual void handleEvent(WebCore::Event*);
    virtual void frameRectsChanged();
    virtual void setParentVisible(bool);
    virtual void setParent(WebCore::ScrollView*);
    virtual void widgetPositionsUpdated();
    virtual bool isPluginContainer() const { return true; }

    // WebPluginContainer methods
    virtual WebElement element();
    virtual void invalidate();
    virtual void invalidateRect(const WebRect&);
    virtual void scrollRect(int dx, int dy, const WebRect&);
    virtual void reportGeometry();
    virtual void setBackingTextureId(unsigned);
    virtual void setBackingIOSurfaceId(int width,
                                       int height,
                                       uint32_t ioSurfaceId);
    virtual void commitBackingTexture();
    virtual void clearScriptObjects();
    virtual NPObject* scriptableObjectForElement();
    virtual WebString executeScriptURL(const WebURL&, bool popupsAllowed);
    virtual void loadFrameRequest(const WebURLRequest&, const WebString& target, bool notifyNeeded, void* notifyData);
    virtual void zoomLevelChanged(double zoomLevel);    
    virtual void setOpaque(bool);
    virtual bool isRectTopmost(const WebRect&);

    // This cannot be null.
    WebPlugin* plugin() { return m_webPlugin; }
    void setPlugin(WebPlugin*);

    // Printing interface. The plugin can support custom printing
    // (which means it controls the layout, number of pages etc).
    // Whether the plugin supports its own paginated print. The other print
    // interface methods are called only if this method returns true.
    bool supportsPaginatedPrint() const;
    // If the plugin content should not be scaled to the printable area of
    // the page, then this method should return true.
    bool isPrintScalingDisabled() const;
    // Sets up printing at the specified WebPrintParams. Returns the number of pages to be printed at these settings.
    int printBegin(const WebPrintParams&) const;
    // Prints the page specified by pageNumber (0-based index) into the supplied canvas.
    bool printPage(int pageNumber, WebCore::GraphicsContext* gc);
    // Ends the print operation.
    void printEnd();

    // Copy the selected text.
    void copy();

    // Resource load events for the plugin's source data:
    void didReceiveResponse(const WebCore::ResourceResponse&);
    void didReceiveData(const char *data, int dataLength);
    void didFinishLoading();
    void didFailLoading(const WebCore::ResourceError&);

    NPObject* scriptableObject();

    void willDestroyPluginLoadObserver(WebPluginLoadObserver*);

#if USE(ACCELERATED_COMPOSITING)
    virtual WebCore::LayerChromium* platformLayer() const;
#endif

    ScrollbarGroup* scrollbarGroup();

    void willStartLiveResize();
    void willEndLiveResize();

    bool paintCustomOverhangArea(WebCore::GraphicsContext*, const WebCore::IntRect&, const WebCore::IntRect&, const WebCore::IntRect&);

private:
    WebPluginContainerImpl(WebCore::HTMLPlugInElement* element, WebPlugin* webPlugin);
    ~WebPluginContainerImpl();

    void handleMouseEvent(WebCore::MouseEvent*);
    void handleWheelEvent(WebCore::WheelEvent*);
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

#if USE(ACCELERATED_COMPOSITING)
    // A composited plugin will either have no composited layer, a texture layer, or an IOSurface layer.
    // It will never have both a texture and IOSurface output.
    unsigned m_textureId;
    WebExternalTextureLayer m_textureLayer;

    unsigned m_ioSurfaceId;
    WebIOSurfaceLayer m_ioSurfaceLayer;
#endif

    // The associated scrollbar group object, created lazily. Used for Pepper
    // scrollbars.
    OwnPtr<ScrollbarGroup> m_scrollbarGroup;
};

} // namespace WebKit

#endif
