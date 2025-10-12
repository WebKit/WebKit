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

#include <WebCore/HTMLFrameOwnerElement.h>
#include <WebCore/Image.h>
#include <WebCore/JSValueInWrappedObject.h>
#include <WebCore/RenderEmbeddedObject.h>
#include <wtf/Platform.h>

namespace JSC {
namespace Bindings {
class Instance;
}
}

namespace WebCore {

class HTMLImageLoader;
class PluginReplacement;
class PluginViewBase;
class RenderWidget;
class VoidCallback;

enum class CreatePlugins : bool { No, Yes };

class HTMLPlugInElement : public HTMLFrameOwnerElement {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(HTMLPlugInElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(HTMLPlugInElement);
public:
    virtual ~HTMLPlugInElement();

    void resetInstance();

    JSC::Bindings::Instance* bindingsInstance();

    enum class PluginLoadingPolicy { DoNotLoad, Load };
    WEBCORE_EXPORT PluginViewBase* pluginWidget(PluginLoadingPolicy = PluginLoadingPolicy::Load) const;

    enum class DisplayState : uint8_t {
        Playing,
        PreparingPluginReplacement,
        DisplayingPluginReplacement,
    };
    DisplayState displayState() const { return m_displayState; }
    void setDisplayState(DisplayState);

    bool isCapturingMouseEvents() const { return m_isCapturingMouseEvents; }
    void setIsCapturingMouseEvents(bool capturing) { m_isCapturingMouseEvents = capturing; }

#if PLATFORM(IOS_FAMILY)
    bool willRespondToMouseMoveEvents() const final { return false; }
#endif
    bool willRespondToMouseClickEventsWithEditability(Editability) const final;

    WEBCORE_EXPORT void pluginDestroyedWithPendingPDFTestCallback(RefPtr<VoidCallback>&&);
    WEBCORE_EXPORT RefPtr<VoidCallback> takePendingPDFTestCallback();

    RenderEmbeddedObject* renderEmbeddedObject() const;

    virtual void updateWidget(CreatePlugins) = 0;

    const String& serviceType() const { return m_serviceType; }
    const String& url() const { return m_url; }

    bool needsWidgetUpdate() const { return m_needsWidgetUpdate; }
    void setNeedsWidgetUpdate(bool needsWidgetUpdate) { m_needsWidgetUpdate = needsWidgetUpdate; }

    bool shouldBypassCSPForPDFPlugin(const String& contentType) const;

protected:
    HTMLPlugInElement(const QualifiedName& tagName, Document&, OptionSet<TypeFlag> = { });

    bool canContainRangeEndPoint() const override { return false; }
    void willDetachRenderers() override;
    bool hasPresentationalHintsForAttribute(const QualifiedName&) const override;
    void collectPresentationalHintsForAttribute(const QualifiedName&, const AtomString&, MutableStyleProperties&) override;

    virtual bool useFallbackContent() const { return false; }

    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode& parentOfInsertedTree) override;
    void removedFromAncestor(RemovalType, ContainerNode& oldParentOfRemovedTree) override;

    void defaultEventHandler(Event&) final;

    void didMoveToNewDocument(Document& oldDocument, Document& newDocument) override;

    bool requestObject(const String& url, const String& mimeType, const Vector<AtomString>& paramNames, const Vector<AtomString>& paramValues);
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) override;
    bool isReplaced(const RenderStyle* = nullptr) const final;
    void didAddUserAgentShadowRoot(ShadowRoot&) final;

    // This will load the plugin if necessary.
    virtual RenderWidget* renderWidgetLoadingPlugin() const;

    bool isImageType();
    HTMLImageLoader* imageLoader() { return m_imageLoader.get(); }
    void updateImageLoaderWithNewURLSoon();

    bool canLoadURL(const String& relativeURL) const;
    bool wouldLoadAsPlugIn(const String& relativeURL, const String& serviceType);

    void scheduleUpdateForAfterStyleResolution();

    String m_serviceType;
    String m_url;

private:
    void swapRendererTimerFired();
    bool shouldOverridePlugin(const String& url, const String& mimeType);

    bool supportsFocus() const final;

    bool isKeyboardFocusable(const FocusEventData&) const final;
    bool isPluginElement() const final;
    bool canLoadScriptURL(const URL&) const final;

    bool canLoadPlugInContent(const String& relativeURL, const String& mimeType) const;
    bool canLoadURL(const URL&) const;

    RenderPtr<RenderElement> createPluginRenderer(RenderStyle&&, const RenderTreePosition&);
    bool childShouldCreateRenderer(const Node&) const override;
    void willRecalcStyle(OptionSet<Style::Change>) final;
    void didRecalcStyle(OptionSet<Style::Change>) final;
    void didAttachRenderers() final;

    void prepareForDocumentSuspension() final;
    void resumeFromDocumentSuspension() final;

    void updateAfterStyleResolution();

    constexpr static OptionSet<TypeFlag> pluginElementTypeFlags();

    RefPtr<JSC::Bindings::Instance> m_instance;
    Timer m_swapRendererTimer;
    RefPtr<PluginReplacement> m_pluginReplacement;
    bool m_isCapturingMouseEvents { false };
    DisplayState m_displayState { DisplayState::Playing };

    RefPtr<VoidCallback> m_pendingPDFTestCallback;

    bool m_needsWidgetUpdate { false };
    bool m_needsDocumentActivationCallbacks { false };
    const std::unique_ptr<HTMLImageLoader> m_imageLoader;
    bool m_needsImageReload { false };
    bool m_hasUpdateScheduledForAfterStyleResolution { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::HTMLPlugInElement)
    static bool isType(const WebCore::Node& node) { return node.isPluginElement(); }
SPECIALIZE_TYPE_TRAITS_END()
