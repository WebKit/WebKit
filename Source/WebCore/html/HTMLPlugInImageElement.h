/*
 * Copyright (C) 2008, 2009, 2011, 2012 Apple Inc. All rights reserved.
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

#include "RenderStyle.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class HTMLImageLoader;
class HTMLVideoElement;
class FrameLoader;
class Image;
class MouseEvent;
class Widget;

enum PluginCreationOption {
    CreateAnyWidgetType,
    CreateOnlyNonNetscapePlugins,
};

enum PreferPlugInsForImagesOption {
    ShouldPreferPlugInsForImages,
    ShouldNotPreferPlugInsForImages
};

// Base class for HTMLObjectElement and HTMLEmbedElement
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
    HTMLPlugInImageElement(const QualifiedName& tagName, Document&, bool createdByParser, PreferPlugInsForImagesOption);

    bool isImageType();

    OwnPtr<HTMLImageLoader> m_imageLoader;
    String m_serviceType;
    String m_url;
    URL m_loadedUrl;

    static void updateWidgetCallback(Node&, unsigned);
    static void startLoadingImageCallback(Node&, unsigned);

    virtual void didAttachRenderers() override;
    virtual void willDetachRenderers() override;

    bool allowedToLoadFrameURL(const String& url);
    bool wouldLoadAsNetscapePlugin(const String& url, const String& serviceType);

    virtual void didMoveToNewDocument(Document* oldDocument) override;

    virtual void documentWillSuspendForPageCache() override;
    virtual void documentDidResumeFromPageCache() override;

    virtual bool isRestartedPlugin() const override { return m_isRestartedPlugin; }
    virtual bool requestObject(const String& url, const String& mimeType, const Vector<String>& paramNames, const Vector<String>& paramValues) override;

private:
    virtual RenderPtr<RenderElement> createElementRenderer(PassRef<RenderStyle>) override;
    virtual bool willRecalcStyle(Style::Change) override;

    virtual void didAddUserAgentShadowRoot(ShadowRoot*) override;

    virtual void finishParsingChildren() override;

    void updateWidgetIfNecessary();
    void startLoadingImage();

    virtual void updateSnapshot(PassRefPtr<Image>) override;
    virtual void dispatchPendingMouseClick() override;
    void simulatedMouseClickTimerFired(DeferrableOneShotTimer<HTMLPlugInImageElement>&);

    void restartSimilarPlugIns();

    virtual bool isPlugInImageElement() const override { return true; }

    void removeSnapshotTimerFired(Timer<HTMLPlugInImageElement>&);

    virtual void defaultEventHandler(Event*) override;

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
NODE_TYPE_CASTS(HTMLPlugInImageElement)

} // namespace WebCore

#endif // HTMLPlugInImageElement_h
