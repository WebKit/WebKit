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

enum PluginCreationOption {
    CreateAnyWidgetType,
    CreateOnlyNonNetscapePlugins,
};

// Base class for HTMLAppletElement, HTMLEmbedElement, and HTMLObjectElement.
// FIXME: Should HTMLAppletElement inherit from HTMLPlugInElement directly instead?
class HTMLPlugInImageElement : public HTMLPlugInElement {
public:
    virtual ~HTMLPlugInImageElement();

    RenderEmbeddedObject* renderEmbeddedObject() const;

    virtual void setDisplayState(DisplayState) override;

    virtual void updateWidget(PluginCreationOption) = 0;

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

    bool shouldPreferPlugInsForImages() const { return m_shouldPreferPlugInsForImages; }

    // Public for FrameView::addWidgetToUpdate()
    bool needsWidgetUpdate() const { return m_needsWidgetUpdate; }
    void setNeedsWidgetUpdate(bool needsWidgetUpdate) { m_needsWidgetUpdate = needsWidgetUpdate; }
    
#if PLATFORM(IOS)
    void createShadowIFrameSubtree(const String& src);
#endif

    void userDidClickSnapshot(PassRefPtr<MouseEvent>, bool forwardEvent);
    void checkSnapshotStatus();
    Image* snapshotImage() const { return m_snapshotImage.get(); }
    void restartSnapshottedPlugIn();

    // Plug-in URL might not be the same as url() with overriding parameters.
    void subframeLoaderWillCreatePlugIn(const URL& plugInURL);
    void subframeLoaderDidCreatePlugIn(const Widget*);

    void setIsPrimarySnapshottedPlugIn(bool);
    bool partOfSnapshotOverlay(Node*);

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
    enum PreferPlugInsForImagesOption { ShouldPreferPlugInsForImages, ShouldNotPreferPlugInsForImages };
    HTMLPlugInImageElement(const QualifiedName& tagName, Document&, bool createdByParser, PreferPlugInsForImagesOption);

    virtual void didMoveToNewDocument(Document* oldDocument) override;
    virtual bool requestObject(const String& url, const String& mimeType, const Vector<String>& paramNames, const Vector<String>& paramValues) override final;

    bool isImageType();
    HTMLImageLoader* imageLoader() { return m_imageLoader.get(); }

    bool allowedToLoadFrameURL(const String& url);
    bool wouldLoadAsNetscapePlugin(const String& url, const String& serviceType);

    String m_serviceType;
    String m_url;

    std::unique_ptr<HTMLImageLoader> m_imageLoader;

private:
    virtual bool isPlugInImageElement() const override final { return true; }
    virtual bool isRestartedPlugin() const override final { return m_isRestartedPlugin; }

    virtual void finishParsingChildren() override final;
    virtual void didAddUserAgentShadowRoot(ShadowRoot*) override final;

    virtual RenderPtr<RenderElement> createElementRenderer(PassRef<RenderStyle>) override;
    virtual bool willRecalcStyle(Style::Change) override final;
    virtual void didAttachRenderers() override final;
    virtual void willDetachRenderers() override final;

    virtual void documentWillSuspendForPageCache() override final;
    virtual void documentDidResumeFromPageCache() override final;

    virtual void defaultEventHandler(Event*) override final;
    virtual void dispatchPendingMouseClick() override final;

    virtual void updateSnapshot(PassRefPtr<Image>) override final;

    void startLoadingImage();
    void updateWidgetIfNecessary();

    void simulatedMouseClickTimerFired(DeferrableOneShotTimer<HTMLPlugInImageElement>&);

    void restartSimilarPlugIns();
    void removeSnapshotTimerFired(Timer<HTMLPlugInImageElement>&);

    URL m_loadedUrl;
    bool m_needsWidgetUpdate;
    bool m_shouldPreferPlugInsForImages;
    bool m_needsDocumentActivationCallbacks;
    RefPtr<MouseEvent> m_pendingClickEventFromSnapshot;
    DeferrableOneShotTimer<HTMLPlugInImageElement> m_simulatedMouseClickTimer;
    Timer<HTMLPlugInImageElement> m_removeSnapshotTimer;
    RefPtr<Image> m_snapshotImage;
    bool m_createdDuringUserGesture;
    bool m_isRestartedPlugin;
    bool m_needsCheckForSizeChange;
    bool m_plugInWasCreated;
    bool m_deferredPromotionToPrimaryPlugIn;
    IntSize m_sizeWhenSnapshotted;
    SnapshotDecision m_snapshotDecision;
};

void isHTMLPlugInImageElement(const HTMLPlugInImageElement&); // Catch unnecessary runtime check of type known at compile time.
inline bool isHTMLPlugInImageElement(const HTMLPlugInElement& element) { return element.isPlugInImageElement(); }
inline bool isHTMLPlugInImageElement(const Node& node) { return node.isPluginElement() && toHTMLPlugInElement(node).isPlugInImageElement(); }
template <> inline bool isElementOfType<const HTMLPlugInImageElement>(const Element& element) { return isHTMLPlugInImageElement(element); }
NODE_TYPE_CASTS(HTMLPlugInImageElement)

} // namespace WebCore

#endif // HTMLPlugInImageElement_h
