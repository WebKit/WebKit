/*
 * Copyright (C) 2008-2019 Apple Inc. All rights reserved.
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

#include "HTMLPlugInElement.h"

namespace WebCore {

class HTMLImageLoader;
class MouseEvent;

enum class CreatePlugins { No, Yes };

// Base class for HTMLAppletElement, HTMLEmbedElement, and HTMLObjectElement.
// FIXME: Perhaps HTMLAppletElement should inherit from HTMLPlugInElement directly instead.
class HTMLPlugInImageElement : public HTMLPlugInElement {
    WTF_MAKE_ISO_ALLOCATED(HTMLPlugInImageElement);
public:
    virtual ~HTMLPlugInImageElement();

    RenderEmbeddedObject* renderEmbeddedObject() const;

    virtual void updateWidget(CreatePlugins) = 0;

    const String& serviceType() const { return m_serviceType; }
    const String& url() const { return m_url; }
    const URL& loadedUrl() const { return m_loadedUrl; }

    // Public for FrameView::addWidgetToUpdate()
    bool needsWidgetUpdate() const { return m_needsWidgetUpdate; }
    void setNeedsWidgetUpdate(bool needsWidgetUpdate) { m_needsWidgetUpdate = needsWidgetUpdate; }
    
    void userDidClickSnapshot(MouseEvent&, bool forwardEvent);
    void checkSnapshotStatus();
    Image* snapshotImage() const { return m_snapshotImage.get(); }
    WEBCORE_EXPORT void restartSnapshottedPlugIn();

    // Plug-in URL might not be the same as url() with overriding parameters.
    void subframeLoaderWillCreatePlugIn(const URL& plugInURL);
    void subframeLoaderDidCreatePlugIn(const Widget&);

    WEBCORE_EXPORT void setIsPrimarySnapshottedPlugIn(bool);
    bool partOfSnapshotOverlay(const EventTarget*) const;

    bool needsCheckForSizeChange() const { return m_needsCheckForSizeChange; }
    void setNeedsCheckForSizeChange() { m_needsCheckForSizeChange = true; }
    void checkSizeChangeForSnapshotting();

    enum SnapshotDecision {
        SnapshotNotYetDecided,
        NeverSnapshot,
        Snapshotted,
        MaySnapshotWhenResized,
        MaySnapshotWhenContentIsSet
    };
    SnapshotDecision snapshotDecision() const { return m_snapshotDecision; }

protected:
    HTMLPlugInImageElement(const QualifiedName& tagName, Document&);
    void finishCreating();

    void didMoveToNewDocument(Document& oldDocument, Document& newDocument) override;

    bool requestObject(const String& url, const String& mimeType, const Vector<String>& paramNames, const Vector<String>& paramValues) final;

    bool isImageType();
    HTMLImageLoader* imageLoader() { return m_imageLoader.get(); }
    void updateImageLoaderWithNewURLSoon();

    bool canLoadURL(const String& relativeURL) const;
    bool wouldLoadAsPlugIn(const String& relativeURL, const String& serviceType);

    void scheduleUpdateForAfterStyleResolution();

    String m_serviceType;
    String m_url;

private:
    bool isPlugInImageElement() const final { return true; }
    bool isRestartedPlugin() const final { return m_isRestartedPlugin; }

    bool canLoadPlugInContent(const String& relativeURL, const String& mimeType) const;
    bool canLoadURL(const URL&) const;

    void didAddUserAgentShadowRoot(ShadowRoot&) final;

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) override;
    bool childShouldCreateRenderer(const Node&) const override;
    void willRecalcStyle(Style::Change) final;
    void didRecalcStyle(Style::Change) final;
    void didAttachRenderers() final;
    void willDetachRenderers() final;

    void prepareForDocumentSuspension() final;
    void resumeFromDocumentSuspension() final;

    void defaultEventHandler(Event&) final;
    void dispatchPendingMouseClick() final;

    void updateSnapshot(Image*) final;

    void updateAfterStyleResolution();

    void simulatedMouseClickTimerFired();

    void restartSimilarPlugIns();
    void removeSnapshotTimerFired();
    bool isTopLevelFullPagePlugin(const RenderEmbeddedObject&) const;

    void setDisplayState(DisplayState) final;

    URL m_loadedUrl;
    bool m_needsWidgetUpdate { false };
    bool m_needsDocumentActivationCallbacks { false };
    RefPtr<MouseEvent> m_pendingClickEventFromSnapshot;
    DeferrableOneShotTimer m_simulatedMouseClickTimer;
    Timer m_removeSnapshotTimer;
    RefPtr<Image> m_snapshotImage;
    bool m_createdDuringUserGesture { false };
    bool m_isRestartedPlugin { false };
    bool m_needsCheckForSizeChange { false };
    bool m_plugInWasCreated { false };
    bool m_deferredPromotionToPrimaryPlugIn { false };
    IntSize m_sizeWhenSnapshotted;
    SnapshotDecision m_snapshotDecision { SnapshotNotYetDecided };
    bool m_plugInDimensionsSpecified { false };
    std::unique_ptr<HTMLImageLoader> m_imageLoader;
    bool m_needsImageReload { false };
    bool m_hasUpdateScheduledForAfterStyleResolution { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::HTMLPlugInImageElement)
    static bool isType(const WebCore::HTMLPlugInElement& element) { return element.isPlugInImageElement(); }
    static bool isType(const WebCore::Node& node) { return is<WebCore::HTMLPlugInElement>(node) && isType(downcast<WebCore::HTMLPlugInElement>(node)); }
SPECIALIZE_TYPE_TRAITS_END()
