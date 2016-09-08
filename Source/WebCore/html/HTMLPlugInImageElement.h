/*
 * Copyright (C) 2008, 2009, 2011, 2012, 2014 Apple Inc. All rights reserved.
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

#ifndef HTMLPlugInImageElement_h
#define HTMLPlugInImageElement_h

#include "HTMLPlugInElement.h"

namespace WebCore {

class HTMLImageLoader;
class FrameLoader;
class Image;
class MouseEvent;
class RenderStyle;
class Widget;

enum class CreatePlugins {
    No,
    Yes,
};

// Base class for HTMLAppletElement, HTMLEmbedElement, and HTMLObjectElement.
// FIXME: Should HTMLAppletElement inherit from HTMLPlugInElement directly instead?
class HTMLPlugInImageElement : public HTMLPlugInElement {
public:
    virtual ~HTMLPlugInImageElement();

    RenderEmbeddedObject* renderEmbeddedObject() const;

    void setDisplayState(DisplayState) override;

    virtual void updateWidget(CreatePlugins) = 0;

    const String& serviceType() const { return m_serviceType; }
    const String& url() const { return m_url; }
    const URL& loadedUrl() const { return m_loadedUrl; }

    const String loadedMimeType() const
    {
        String mimeType = serviceType();
        if (mimeType.isEmpty())
            mimeType = mimeTypeFromURL(m_loadedUrl);
        return mimeType;
    }

    // Public for FrameView::addWidgetToUpdate()
    bool needsWidgetUpdate() const { return m_needsWidgetUpdate; }
    void setNeedsWidgetUpdate(bool needsWidgetUpdate) { m_needsWidgetUpdate = needsWidgetUpdate; }
    
    void userDidClickSnapshot(PassRefPtr<MouseEvent>, bool forwardEvent);
    void checkSnapshotStatus();
    Image* snapshotImage() const { return m_snapshotImage.get(); }
    WEBCORE_EXPORT void restartSnapshottedPlugIn();

    // Plug-in URL might not be the same as url() with overriding parameters.
    void subframeLoaderWillCreatePlugIn(const URL& plugInURL);
    void subframeLoaderDidCreatePlugIn(const Widget&);

    WEBCORE_EXPORT void setIsPrimarySnapshottedPlugIn(bool);
    bool partOfSnapshotOverlay(const Node*) const;

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
    HTMLPlugInImageElement(const QualifiedName& tagName, Document&, bool createdByParser);

    void didMoveToNewDocument(Document* oldDocument) override;
    bool requestObject(const String& url, const String& mimeType, const Vector<String>& paramNames, const Vector<String>& paramValues) final;

    bool isImageType();
    HTMLImageLoader* imageLoader() { return m_imageLoader.get(); }

    bool allowedToLoadFrameURL(const String& url);
    bool wouldLoadAsPlugIn(const String& url, const String& serviceType);

    String m_serviceType;
    String m_url;

    std::unique_ptr<HTMLImageLoader> m_imageLoader;

private:
    bool isPlugInImageElement() const final { return true; }
    bool isRestartedPlugin() const final { return m_isRestartedPlugin; }

    bool allowedToLoadPluginContent(const String& url, const String& mimeType) const;

    void finishParsingChildren() final;
    void didAddUserAgentShadowRoot(ShadowRoot*) final;

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) override;
    bool childShouldCreateRenderer(const Node&) const override;
    bool willRecalcStyle(Style::Change) final;
    void didAttachRenderers() final;
    void willDetachRenderers() final;

    void prepareForDocumentSuspension() final;
    void resumeFromDocumentSuspension() final;

    void defaultEventHandler(Event&) final;
    void dispatchPendingMouseClick() final;

    void updateSnapshot(PassRefPtr<Image>) final;

    void startLoadingImage();
    void updateWidgetIfNecessary();

    void simulatedMouseClickTimerFired();

    void restartSimilarPlugIns();
    void removeSnapshotTimerFired();
    bool isTopLevelFullPagePlugin(const RenderEmbeddedObject&) const;

    URL m_loadedUrl;
    bool m_needsWidgetUpdate;
    bool m_needsDocumentActivationCallbacks;
    RefPtr<MouseEvent> m_pendingClickEventFromSnapshot;
    DeferrableOneShotTimer m_simulatedMouseClickTimer;
    Timer m_removeSnapshotTimer;
    RefPtr<Image> m_snapshotImage;
    bool m_createdDuringUserGesture;
    bool m_isRestartedPlugin;
    bool m_needsCheckForSizeChange;
    bool m_plugInWasCreated;
    bool m_deferredPromotionToPrimaryPlugIn;
    IntSize m_sizeWhenSnapshotted;
    SnapshotDecision m_snapshotDecision;
    bool m_plugInDimensionsSpecified;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::HTMLPlugInImageElement)
    static bool isType(const WebCore::HTMLPlugInElement& element) { return element.isPlugInImageElement(); }
    static bool isType(const WebCore::Node& node) { return is<WebCore::HTMLPlugInElement>(node) && isType(downcast<WebCore::HTMLPlugInElement>(node)); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // HTMLPlugInImageElement_h
