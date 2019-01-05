/*
 * Copyright (C) 2008-2017 Apple Inc. All rights reserved.
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

#import "WebChromeClientIOS.h"

#if PLATFORM(IOS_FAMILY)

#import "DOMNodeInternal.h"
#import "PopupMenuIOS.h"
#import "SearchPopupMenuIOS.h"
#import "WebDelegateImplementationCaching.h"
#import "WebFixedPositionContent.h"
#import "WebFixedPositionContentInternal.h"
#import "WebFormDelegate.h"
#import "WebFrameIOS.h"
#import "WebFrameInternal.h"
#import "WebHistoryItemInternal.h"
#import "WebOpenPanelResultListener.h"
#import "WebUIDelegate.h"
#import "WebUIDelegatePrivate.h"
#import "WebUIKitDelegate.h"
#import "WebView.h"
#import "WebViewInternal.h"
#import "WebViewPrivate.h"
#import <WebCore/DisabledAdaptations.h>
#import <WebCore/FileChooser.h>
#import <WebCore/FloatRect.h>
#import <WebCore/Frame.h>
#import <WebCore/GraphicsLayer.h>
#import <WebCore/HTMLInputElement.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/IntRect.h>
#import <WebCore/Node.h>
#import <WebCore/PlatformScreen.h>
#import <WebCore/RenderBox.h>
#import <WebCore/RenderObject.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/ScrollingConstraints.h>
#import <WebCore/WAKWindow.h>
#import <WebCore/WebCoreThreadMessage.h>
#import <wtf/HashMap.h>
#import <wtf/RefPtr.h>

NSString * const WebOpenPanelConfigurationAllowMultipleFilesKey = @"WebOpenPanelConfigurationAllowMultipleFilesKey";
NSString * const WebOpenPanelConfigurationMediaCaptureTypeKey = @"WebOpenPanelConfigurationMediaCaptureTypeKey";
NSString * const WebOpenPanelConfigurationMimeTypesKey = @"WebOpenPanelConfigurationMimeTypesKey";

using namespace WebCore;

#if ENABLE(MEDIA_CAPTURE)

static WebMediaCaptureType webMediaCaptureType(MediaCaptureType type)
{
    switch (type) {
    case MediaCaptureTypeNone:
        return WebMediaCaptureTypeNone;
    case MediaCaptureTypeUser:
        return WebMediaCaptureTypeUser;
    case MediaCaptureTypeEnvironment:
        return WebMediaCaptureTypeEnvironment;
    }

    ASSERT_NOT_REACHED();
    return WebMediaCaptureTypeNone;
}

#endif

void WebChromeClientIOS::setWindowRect(const WebCore::FloatRect& r)
{
    [[webView() _UIDelegateForwarder] webView:webView() setFrame:r];
}

FloatRect WebChromeClientIOS::windowRect()
{
    CGRect windowRect = [[webView() _UIDelegateForwarder] webViewFrame:webView()];
    return enclosingIntRect(windowRect);
}

void WebChromeClientIOS::focus()
{
    [[webView() _UIDelegateForwarder] webViewFocus:webView()];
}

void WebChromeClientIOS::runJavaScriptAlert(Frame& frame, const WTF::String& message)
{
    WebThreadLockPushModal();
    [[webView() _UIDelegateForwarder] webView:webView() runJavaScriptAlertPanelWithMessage:message initiatedByFrame:kit(&frame)];
    WebThreadLockPopModal();
}

bool WebChromeClientIOS::runJavaScriptConfirm(Frame& frame, const WTF::String& message)
{
    WebThreadLockPushModal();
    bool result = [[webView() _UIDelegateForwarder] webView:webView() runJavaScriptConfirmPanelWithMessage:message initiatedByFrame:kit(&frame)];
    WebThreadLockPopModal();
    return result;
}

bool WebChromeClientIOS::runJavaScriptPrompt(Frame& frame, const WTF::String& prompt, const WTF::String& defaultText, WTF::String& result)
{
    WebThreadLockPushModal();
    result = [[webView() _UIDelegateForwarder] webView:webView() runJavaScriptTextInputPanelWithPrompt:prompt defaultText:defaultText initiatedByFrame:kit(&frame)];
    WebThreadLockPopModal();
    return !result.isNull();
}

void WebChromeClientIOS::runOpenPanel(Frame&, FileChooser& chooser)
{
    auto& settings = chooser.settings();
    BOOL allowMultipleFiles = settings.allowsMultipleFiles;
    WebOpenPanelResultListener *listener = [[WebOpenPanelResultListener alloc] initWithChooser:chooser];

    NSMutableArray *mimeTypes = [NSMutableArray arrayWithCapacity:settings.acceptMIMETypes.size()];
    for (auto& type : settings.acceptMIMETypes)
        [mimeTypes addObject:type];

    WebMediaCaptureType captureType = WebMediaCaptureTypeNone;
#if ENABLE(MEDIA_CAPTURE)
    captureType = webMediaCaptureType(settings.mediaCaptureType);
#endif
    NSDictionary *configuration = @{
        WebOpenPanelConfigurationAllowMultipleFilesKey: @(allowMultipleFiles),
        WebOpenPanelConfigurationMimeTypesKey: mimeTypes,
        WebOpenPanelConfigurationMediaCaptureTypeKey: @(captureType)
    };

    if (WebThreadIsCurrent()) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [[webView() _UIKitDelegateForwarder] webView:webView() runOpenPanelForFileButtonWithResultListener:listener configuration:configuration];
        });
    } else
        [[webView() _UIKitDelegateForwarder] webView:webView() runOpenPanelForFileButtonWithResultListener:listener configuration:configuration];

    [listener release];
}

void WebChromeClientIOS::showShareSheet(ShareDataWithParsedURL&, CompletionHandler<void(bool)>&&)
{
}

#if ENABLE(IOS_TOUCH_EVENTS)

void WebChromeClientIOS::didPreventDefaultForEvent()
{
    [[webView() _UIKitDelegateForwarder] webViewDidPreventDefaultForEvent:webView()];
}

#endif

void WebChromeClientIOS::didReceiveMobileDocType(bool isMobileDoctype)
{
    if (isMobileDoctype)
        [[webView() _UIKitDelegateForwarder] webViewDidReceiveMobileDocType:webView()];
}

void WebChromeClientIOS::setNeedsScrollNotifications(WebCore::Frame& frame, bool flag)
{
    [[webView() _UIKitDelegateForwarder] webView:webView() needsScrollNotifications:[NSNumber numberWithBool:flag] forFrame:kit(&frame)];
}

void WebChromeClientIOS::observedContentChange(WebCore::Frame& frame)
{
    [[webView() _UIKitDelegateForwarder] webView:webView() didObserveDeferredContentChange:WKObservedContentChange() forFrame:kit(&frame)];
}

void WebChromeClientIOS::clearContentChangeObservers(WebCore::Frame& frame)
{
    ASSERT(WebThreadCountOfObservedDOMTimers() > 0);
    if (WebThreadCountOfObservedDOMTimers() > 0) {
        WebThreadClearObservedDOMTimers();
        observedContentChange(frame);
    }
}

static inline NSString *nameForViewportFitValue(ViewportFit value)
{
    switch (value) {
    case ViewportFit::Auto:
        return WebViewportFitAutoValue;
    case ViewportFit::Contain:
        return WebViewportFitContainValue;
    case ViewportFit::Cover:
        return WebViewportFitCoverValue;
    }
    return WebViewportFitAutoValue;
}

static inline NSDictionary *dictionaryForViewportArguments(const WebCore::ViewportArguments& arguments)
{
    return @{ WebViewportInitialScaleKey: @(arguments.zoom),
              WebViewportMinimumScaleKey: @(arguments.minZoom),
              WebViewportMaximumScaleKey: @(arguments.maxZoom),
              WebViewportUserScalableKey: @(arguments.userZoom),
              WebViewportShrinkToFitKey: @(0),
              WebViewportFitKey: nameForViewportFitValue(arguments.viewportFit),
              WebViewportWidthKey: @(arguments.width),
              WebViewportHeightKey: @(arguments.height) };
}

FloatSize WebChromeClientIOS::screenSize() const
{
    return FloatSize(WebCore::screenSize());
}

FloatSize WebChromeClientIOS::availableScreenSize() const
{
    // WebKit1 code should query the WAKWindow for the available screen size.
    ASSERT_NOT_REACHED();
    return FloatSize();
}

FloatSize WebChromeClientIOS::overrideScreenSize() const
{
    return screenSize();
}

void WebChromeClientIOS::dispatchViewportPropertiesDidChange(const WebCore::ViewportArguments& arguments) const
{
    [[webView() _UIKitDelegateForwarder] webView:webView() didReceiveViewportArguments:dictionaryForViewportArguments(arguments)];
}

void WebChromeClientIOS::dispatchDisabledAdaptationsDidChange(const OptionSet<WebCore::DisabledAdaptations>&) const
{
}

void WebChromeClientIOS::notifyRevealedSelectionByScrollingFrame(WebCore::Frame& frame)
{
    [[webView() _UIKitDelegateForwarder] revealedSelectionByScrollingWebFrame:kit(&frame)];
}

bool WebChromeClientIOS::isStopping()
{
    return [webView() _isStopping];
}

void WebChromeClientIOS::didLayout(LayoutType changeType)
{
    [[webView() _UIKitDelegate] webThreadWebViewDidLayout:webView() byScrolling:(changeType == ChromeClient::Scroll)];
}

void WebChromeClientIOS::didStartOverflowScroll()
{
    [[[webView() _UIKitDelegateForwarder] asyncForwarder] webViewDidStartOverflowScroll:webView()];
}

void WebChromeClientIOS::didEndOverflowScroll()
{
    [[[webView() _UIKitDelegateForwarder] asyncForwarder] webViewDidEndOverflowScroll:webView()];
}

void WebChromeClientIOS::suppressFormNotifications() 
{
    m_formNotificationSuppressions++;
}

void WebChromeClientIOS::restoreFormNotifications() 
{
    m_formNotificationSuppressions--;
    ASSERT(m_formNotificationSuppressions >= 0);
    if (m_formNotificationSuppressions < 0)
        m_formNotificationSuppressions = 0;
}

void WebChromeClientIOS::elementDidFocus(WebCore::Element& element)
{
    if (m_formNotificationSuppressions <= 0)
        [[webView() _UIKitDelegateForwarder] webView:webView() elementDidFocusNode:kit(&element)];
}

void WebChromeClientIOS::elementDidBlur(WebCore::Element& element)
{
    if (m_formNotificationSuppressions <= 0)
        [[webView() _UIKitDelegateForwarder] webView:webView() elementDidBlurNode:kit(&element)];
}

bool WebChromeClientIOS::selectItemWritingDirectionIsNatural()
{
    return false;
}

bool WebChromeClientIOS::selectItemAlignmentFollowsMenuWritingDirection()
{
    return true;
}

RefPtr<WebCore::PopupMenu> WebChromeClientIOS::createPopupMenu(WebCore::PopupMenuClient& client) const
{
    return adoptRef(new PopupMenuIOS(&client));
}

RefPtr<WebCore::SearchPopupMenu> WebChromeClientIOS::createSearchPopupMenu(WebCore::PopupMenuClient& client) const
{
    return adoptRef(new SearchPopupMenuIOS(&client));
}

void WebChromeClientIOS::attachRootGraphicsLayer(Frame&, GraphicsLayer* graphicsLayer)
{
    // FIXME: for non-root frames we rely on RenderView positioning the root layer,
    // which is a hack. <rdar://problem/5906146>
    // Send the delegate message on the web thread to avoid <rdar://problem/8567677>
    [[webView() _UIKitDelegate] _webthread_webView:webView() attachRootLayer:graphicsLayer ? graphicsLayer->platformLayer() : 0];
}

void WebChromeClientIOS::didFlushCompositingLayers()
{
    [[[webView() _UIKitDelegateForwarder] asyncForwarder] webViewDidCommitCompositingLayerChanges:webView()];
}

bool WebChromeClientIOS::fetchCustomFixedPositionLayoutRect(IntRect& rect)
{
    NSRect updatedRect;
    if ([webView() _fetchCustomFixedPositionLayoutRect:&updatedRect]) {
        rect = enclosingIntRect(updatedRect);
        return true;
    }

    return false;
}

void WebChromeClientIOS::updateViewportConstrainedLayers(HashMap<PlatformLayer*, std::unique_ptr<ViewportConstraints>>& layerMap, const HashMap<PlatformLayer*, PlatformLayer*>& stickyContainers)
{
    [[webView() _fixedPositionContent] setViewportConstrainedLayers:layerMap stickyContainerMap:stickyContainers];
}

void WebChromeClientIOS::addOrUpdateScrollingLayer(Node* node, PlatformLayer* scrollingLayer, PlatformLayer* contentsLayer, const IntSize& scrollSize, bool allowHorizontalScrollbar, bool allowVerticalScrollbar)
{
    DOMNode *domNode = kit(node);

    [[[webView() _UIKitDelegateForwarder] asyncForwarder] webView:webView() didCreateOrUpdateScrollingLayer:scrollingLayer withContentsLayer:contentsLayer scrollSize:[NSValue valueWithSize:scrollSize] forNode:domNode
        allowHorizontalScrollbar:allowHorizontalScrollbar allowVerticalScrollbar:allowVerticalScrollbar];
}

void WebChromeClientIOS::removeScrollingLayer(Node* node, PlatformLayer* scrollingLayer, PlatformLayer* contentsLayer)
{
    DOMNode *domNode = kit(node);
    [[[webView() _UIKitDelegateForwarder] asyncForwarder] webView:webView() willRemoveScrollingLayer:scrollingLayer withContentsLayer:contentsLayer forNode:domNode];
}

void WebChromeClientIOS::webAppOrientationsUpdated()
{
    [[webView() _UIDelegateForwarder] webViewSupportedOrientationsUpdated:webView()];
}

void WebChromeClientIOS::focusedElementChanged(Element* element)
{
    if (!is<HTMLInputElement>(element))
        return;

    HTMLInputElement& inputElement = downcast<HTMLInputElement>(*element);
    if (!inputElement.isText())
        return;

    CallFormDelegate(webView(), @selector(didFocusTextField:inFrame:), kit(&inputElement), kit(inputElement.document().frame()));
}

void WebChromeClientIOS::showPlaybackTargetPicker(bool hasVideo, WebCore::RouteSharingPolicy, const String&)
{
    CGPoint point = [[webView() _UIKitDelegateForwarder] interactionLocation];
    CGRect elementRect = [[webView() mainFrame] elementRectAtPoint:point];
    [[webView() _UIKitDelegateForwarder] showPlaybackTargetPicker:hasVideo fromRect:elementRect];
}

RefPtr<Icon> WebChromeClientIOS::createIconForFiles(const Vector<String>& filenames)
{
    return Icon::createIconForFiles(filenames);
}

#if ENABLE(ORIENTATION_EVENTS)
int WebChromeClientIOS::deviceOrientation() const
{
    return [webView() _deviceOrientation];
}
#endif

#endif // PLATFORM(IOS_FAMILY)
