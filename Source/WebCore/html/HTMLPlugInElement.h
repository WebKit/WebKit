/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2009, 2012 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef HTMLPlugInElement_h
#define HTMLPlugInElement_h

#include "HTMLFrameOwnerElement.h"
#include "Image.h"

#if ENABLE(NETSCAPE_PLUGIN_API)
struct NPObject;
#endif

namespace JSC {
namespace Bindings {
class Instance;
}
}

namespace WebCore {

class PluginReplacement;
class RenderEmbeddedObject;
class RenderWidget;
class Widget;

class HTMLPlugInElement : public HTMLFrameOwnerElement {
public:
    virtual ~HTMLPlugInElement();

    void resetInstance();

    PassRefPtr<JSC::Bindings::Instance> getInstance();

    Widget* pluginWidget() const;

    enum DisplayState {
        WaitingForSnapshot,
        DisplayingSnapshot,
        Restarting,
        RestartingWithPendingMouseClick,
        Playing,
        PreparingPluginReplacement,
        DisplayingPluginReplacement,
    };
    DisplayState displayState() const { return m_displayState; }
    virtual void setDisplayState(DisplayState);
    virtual void updateSnapshot(PassRefPtr<Image>) { }
    virtual void dispatchPendingMouseClick() { }
    virtual bool isRestartedPlugin() const { return false; }

    JSC::JSObject* scriptObjectForPluginReplacement();

#if ENABLE(NETSCAPE_PLUGIN_API)
    NPObject* getNPObject();
#endif

    bool isCapturingMouseEvents() const { return m_isCapturingMouseEvents; }
    void setIsCapturingMouseEvents(bool capturing) { m_isCapturingMouseEvents = capturing; }

    virtual bool canContainRangeEndPoint() const override { return false; }

    bool canProcessDrag() const;

#if PLATFORM(IOS)
    virtual bool willRespondToMouseMoveEvents() override { return false; }
#endif
    virtual bool willRespondToMouseClickEvents() override;

    virtual bool isPlugInImageElement() const { return false; }

protected:
    HTMLPlugInElement(const QualifiedName& tagName, Document&);

    virtual void willDetachRenderers() override;
    virtual bool isPresentationAttribute(const QualifiedName&) const override;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStyleProperties&) override;

    virtual bool useFallbackContent() const { return false; }

    virtual void defaultEventHandler(Event*) override;

    virtual bool requestObject(const String& url, const String& mimeType, const Vector<String>& paramNames, const Vector<String>& paramValues);
    virtual RenderPtr<RenderElement> createElementRenderer(PassRef<RenderStyle>) override;
    virtual void didAddUserAgentShadowRoot(ShadowRoot*) override;

    // Subclasses should use guardedDispatchBeforeLoadEvent instead of calling dispatchBeforeLoadEvent directly.
    bool guardedDispatchBeforeLoadEvent(const String& sourceURL);

    bool m_inBeforeLoadEventHandler;

private:
    void swapRendererTimerFired(Timer<HTMLPlugInElement>&);
    bool shouldOverridePlugin(const String& url, const String& mimeType);

    bool dispatchBeforeLoadEvent(const String& sourceURL); // Not implemented, generates a compile error if subclasses call this by mistake.

    virtual bool areAuthorShadowsAllowed() const override { return false; }

    virtual RenderWidget* renderWidgetForJSBindings() const = 0;

    virtual bool supportsFocus() const override;

    virtual bool isKeyboardFocusable(KeyboardEvent*) const override;
    virtual bool isPluginElement() const override final;

    RefPtr<JSC::Bindings::Instance> m_instance;
    Timer<HTMLPlugInElement> m_swapRendererTimer;
    RefPtr<PluginReplacement> m_pluginReplacement;
#if ENABLE(NETSCAPE_PLUGIN_API)
    NPObject* m_NPObject;
#endif
    bool m_isCapturingMouseEvents;

    DisplayState m_displayState;
};

void isHTMLPlugInElement(const HTMLPlugInElement&); // Catch unnecessary runtime check of type known at compile time.
inline bool isHTMLPlugInElement(const Node& node) { return node.isPluginElement(); }
NODE_TYPE_CASTS(HTMLPlugInElement)

} // namespace WebCore

#endif // HTMLPlugInElement_h
