/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
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

#pragma once

#include "HTMLFrameOwnerElement.h"
#include "Image.h"
#include "RenderEmbeddedObject.h"

namespace JSC {
namespace Bindings {
class Instance;
}
}

namespace WebCore {

class PluginReplacement;
class RenderWidget;
class Widget;

class HTMLPlugInElement : public HTMLFrameOwnerElement {
    WTF_MAKE_ISO_ALLOCATED(HTMLPlugInElement);
public:
    virtual ~HTMLPlugInElement();

    void resetInstance();

    JSC::Bindings::Instance* bindingsInstance();

    enum class PluginLoadingPolicy { DoNotLoad, Load };
    WEBCORE_EXPORT Widget* pluginWidget(PluginLoadingPolicy = PluginLoadingPolicy::Load) const;

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
    virtual void updateSnapshot(Image*) { }
    virtual void dispatchPendingMouseClick() { }
    virtual bool isRestartedPlugin() const { return false; }

    JSC::JSObject* scriptObjectForPluginReplacement();

    bool isCapturingMouseEvents() const { return m_isCapturingMouseEvents; }
    void setIsCapturingMouseEvents(bool capturing) { m_isCapturingMouseEvents = capturing; }

    bool canContainRangeEndPoint() const override { return false; }

    bool canProcessDrag() const;

#if PLATFORM(IOS_FAMILY)
    bool willRespondToMouseMoveEvents() override { return false; }
#endif
    bool willRespondToMouseClickEvents() override;

    virtual bool isPlugInImageElement() const { return false; }

    bool isUserObservable() const;

    WEBCORE_EXPORT bool isBelowSizeThreshold() const;

    // Return whether or not the replacement content for blocked plugins is accessible to the user.
    WEBCORE_EXPORT bool setReplacement(RenderEmbeddedObject::PluginUnavailabilityReason, const String& unavailabilityDescription);

    WEBCORE_EXPORT bool isReplacementObscured();

protected:
    HTMLPlugInElement(const QualifiedName& tagName, Document&);

    void willDetachRenderers() override;
    bool isPresentationAttribute(const QualifiedName&) const override;
    void collectStyleForPresentationAttribute(const QualifiedName&, const AtomString&, MutableStyleProperties&) override;

    virtual bool useFallbackContent() const { return false; }

    void defaultEventHandler(Event&) override;

    virtual bool requestObject(const String& url, const String& mimeType, const Vector<String>& paramNames, const Vector<String>& paramValues);
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) override;
    void didAddUserAgentShadowRoot(ShadowRoot&) override;

    // Subclasses should use guardedDispatchBeforeLoadEvent instead of calling dispatchBeforeLoadEvent directly.
    bool guardedDispatchBeforeLoadEvent(const String& sourceURL);

    bool m_inBeforeLoadEventHandler;

private:
    void swapRendererTimerFired();
    bool shouldOverridePlugin(const String& url, const String& mimeType);

    bool dispatchBeforeLoadEvent(const String& sourceURL) = delete; // Generate a compile error if someone calls this by mistake.

    // This will load the plugin if necessary.
    virtual RenderWidget* renderWidgetLoadingPlugin() const = 0;

    bool supportsFocus() const override;

    bool isKeyboardFocusable(KeyboardEvent*) const override;
    bool isPluginElement() const final;
    bool canLoadScriptURL(const URL&) const final;

    RefPtr<JSC::Bindings::Instance> m_instance;
    Timer m_swapRendererTimer;
    RefPtr<PluginReplacement> m_pluginReplacement;
    bool m_isCapturingMouseEvents;

    DisplayState m_displayState;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::HTMLPlugInElement)
    static bool isType(const WebCore::Node& node) { return node.isPluginElement(); }
SPECIALIZE_TYPE_TRAITS_END()
