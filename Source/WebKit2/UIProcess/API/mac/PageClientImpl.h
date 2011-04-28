/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PageClientImpl_h
#define PageClientImpl_h

#include "CorrectionPanel.h"
#include "PageClient.h"
#include <wtf/RetainPtr.h>

@class WKEditorUndoTargetObjC;
@class WKView;

namespace WebKit {

class FindIndicatorWindow;

// NOTE: This does not use String::operator NSString*() since that function
// expects to be called on the thread running WebCore.
NSString* nsStringFromWebCoreString(const String&);

class PageClientImpl : public PageClient {
public:
    static PassOwnPtr<PageClientImpl> create(WKView*);
    virtual ~PageClientImpl();

private:
    PageClientImpl(WKView*);

    virtual PassOwnPtr<DrawingAreaProxy> createDrawingAreaProxy();
    virtual void setViewNeedsDisplay(const WebCore::IntRect&);
    virtual void displayView();
    virtual void scrollView(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset);

    virtual WebCore::IntSize viewSize();
    virtual bool isViewWindowActive();
    virtual bool isViewFocused();
    virtual bool isViewVisible();
    virtual bool isViewInWindow();
    
    virtual void processDidCrash();
    virtual void pageClosed();
    virtual void didRelaunchProcess();
    virtual void toolTipChanged(const String& oldToolTip, const String& newToolTip);
    virtual void setCursor(const WebCore::Cursor&);
    virtual void setViewportArguments(const WebCore::ViewportArguments&);

    virtual void registerEditCommand(PassRefPtr<WebEditCommandProxy>, WebPageProxy::UndoOrRedo);
    virtual void clearAllEditCommands();
    virtual bool canUndoRedo(WebPageProxy::UndoOrRedo);
    virtual void executeUndoRedo(WebPageProxy::UndoOrRedo);
    virtual bool interpretKeyEvent(const NativeWebKeyboardEvent&, Vector<WebCore::KeypressCommand>&);
    virtual bool executeSavedCommandBySelector(const String& selector);
    virtual void setDragImage(const WebCore::IntPoint& clientPosition, PassRefPtr<ShareableBitmap> dragImage, bool isLinkDrag);
    virtual void updateTextInputState(bool updateSecureInputState);
    virtual void resetTextInputState();

    virtual WebCore::FloatRect convertToDeviceSpace(const WebCore::FloatRect&);
    virtual WebCore::FloatRect convertToUserSpace(const WebCore::FloatRect&);
    virtual WebCore::IntRect windowToScreen(const WebCore::IntRect&);
    
    virtual void doneWithKeyEvent(const NativeWebKeyboardEvent&, bool wasEventHandled);

    virtual PassRefPtr<WebPopupMenuProxy> createPopupMenuProxy(WebPageProxy*);
    virtual PassRefPtr<WebContextMenuProxy> createContextMenuProxy(WebPageProxy*);

    void setFindIndicator(PassRefPtr<FindIndicator>, bool fadeOut);

    virtual void enterAcceleratedCompositingMode(const LayerTreeContext&);
    virtual void exitAcceleratedCompositingMode();

    virtual void accessibilityWebProcessTokenReceived(const CoreIPC::DataReference&);    
    virtual void setComplexTextInputEnabled(uint64_t pluginComplexTextInputIdentifier, bool complexTextInputEnabled);

    virtual CGContextRef containingWindowGraphicsContext();

    virtual void didChangeScrollbarsForMainFrame() const;

    virtual void didCommitLoadForMainFrame(bool useCustomRepresentation);
    virtual void didFinishLoadingDataForCustomRepresentation(const String& suggestedFilename, const CoreIPC::DataReference&);

    virtual double customRepresentationZoomFactor();
    virtual void setCustomRepresentationZoomFactor(double);
    virtual void findStringInCustomRepresentation(const String&, FindOptions, unsigned maxMatchCount);
    virtual void countStringMatchesInCustomRepresentation(const String&, FindOptions, unsigned maxMatchCount);

    virtual void flashBackingStoreUpdates(const Vector<WebCore::IntRect>& updateRects);

    virtual void didPerformDictionaryLookup(const String&, double scaleFactor, const DictionaryPopupInfo&);
    virtual void dismissDictionaryLookupPanel();

    virtual void showCorrectionPanel(WebCore::CorrectionPanelInfo::PanelType, const WebCore::FloatRect& boundingBoxOfReplacedString, const String& replacedString, const String& replacementString, const Vector<String>& alternativeReplacementStrings);
    virtual void dismissCorrectionPanel(WebCore::ReasonForDismissingCorrectionPanel);
    virtual String dismissCorrectionPanelSoon(WebCore::ReasonForDismissingCorrectionPanel);
    virtual void recordAutocorrectionResponse(WebCore::EditorClient::AutocorrectionResponseType, const String& replacedString, const String& replacementString);

    virtual float userSpaceScaleFactor() const;
    
    virtual WKView* wkView() const { return m_wkView; }

    WKView* m_wkView;
    RetainPtr<WKEditorUndoTargetObjC> m_undoTarget;
#if !defined(BUILDING_ON_SNOW_LEOPARD)
    CorrectionPanel m_correctionPanel;
#endif
};

} // namespace WebKit

#endif // PageClientImpl_h
