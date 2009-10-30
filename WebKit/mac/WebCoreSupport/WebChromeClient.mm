/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebChromeClient.h"

#import "DOMNodeInternal.h"
#import "WebDefaultUIDelegate.h"
#import "WebDelegateImplementationCaching.h"
#import "WebElementDictionary.h"
#import "WebFrameInternal.h"
#import "WebFrameView.h"
#import "WebGeolocationInternal.h"
#import "WebHTMLViewInternal.h"
#import "WebHistoryInternal.h"
#import "WebKitPrefix.h"
#import "WebKitSystemInterface.h"
#import "WebNSURLRequestExtras.h"
#import "WebPlugin.h"
#import "WebSecurityOriginInternal.h"
#import "WebUIDelegatePrivate.h"
#import "WebView.h"
#import "WebViewInternal.h"
#import <Foundation/Foundation.h>
#import <WebCore/BlockExceptions.h>
#import <WebCore/Console.h>
#import <WebCore/Element.h>
#import <WebCore/FileChooser.h>
#import <WebCore/FloatRect.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoadRequest.h>
#import <WebCore/Geolocation.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/IntRect.h>
#import <WebCore/Page.h>
#import <WebCore/PlatformScreen.h>
#import <WebCore/PlatformString.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/ScrollView.h>
#import <WebCore/Widget.h>
#import <WebCore/WindowFeatures.h>
#import <wtf/PassRefPtr.h>
#import <wtf/Vector.h>

#if USE(ACCELERATED_COMPOSITING)
#import <WebCore/GraphicsLayer.h>
#endif

#if USE(PLUGIN_HOST_PROCESS)
#import "NetscapePluginHostManager.h"
#endif

@interface NSView (WebNSViewDetails)
- (NSView *)_findLastViewInKeyViewLoop;
@end

// For compatibility with old SPI.
@interface NSView (WebOldWebKitPlugInDetails)
- (void)setIsSelected:(BOOL)isSelected;
@end

@interface NSWindow (AppKitSecretsIKnowAbout)
- (NSRect)_growBoxRect;
@end

using namespace WebCore;

@interface WebOpenPanelResultListener : NSObject <WebOpenPanelResultListener> {
    FileChooser* _chooser;
}
- (id)initWithChooser:(PassRefPtr<FileChooser>)chooser;
@end

WebChromeClient::WebChromeClient(WebView *webView) 
    : m_webView(webView)
{
}

void WebChromeClient::chromeDestroyed()
{
    delete this;
}

// These functions scale between window and WebView coordinates because JavaScript/DOM operations 
// assume that the WebView and the window share the same coordinate system.

void WebChromeClient::setWindowRect(const FloatRect& rect)
{
    NSRect windowRect = toDeviceSpace(rect, [m_webView window]);
    [[m_webView _UIDelegateForwarder] webView:m_webView setFrame:windowRect];
}

FloatRect WebChromeClient::windowRect()
{
    NSRect windowRect = [[m_webView _UIDelegateForwarder] webViewFrame:m_webView];
    return toUserSpace(windowRect, [m_webView window]);
}

// FIXME: We need to add API for setting and getting this.
FloatRect WebChromeClient::pageRect()
{
    return [m_webView frame];
}

float WebChromeClient::scaleFactor()
{
    if (NSWindow *window = [m_webView window])
        return [window  userSpaceScaleFactor];
    return [[NSScreen mainScreen] userSpaceScaleFactor];
}

void WebChromeClient::focus()
{
    [[m_webView _UIDelegateForwarder] webViewFocus:m_webView];
}

void WebChromeClient::unfocus()
{
    [[m_webView _UIDelegateForwarder] webViewUnfocus:m_webView];
}

bool WebChromeClient::canTakeFocus(FocusDirection)
{
    // There's unfortunately no way to determine if we will become first responder again
    // once we give it up, so we just have to guess that we won't.
    return true;
}

void WebChromeClient::takeFocus(FocusDirection direction)
{
    if (direction == FocusDirectionForward) {
        // Since we're trying to move focus out of m_webView, and because
        // m_webView may contain subviews within it, we ask it for the next key
        // view of the last view in its key view loop. This makes m_webView
        // behave as if it had no subviews, which is the behavior we want.
        NSView *lastView = [m_webView _findLastViewInKeyViewLoop];
        // avoid triggering assertions if the WebView is the only thing in the key loop
        if ([m_webView _becomingFirstResponderFromOutside] && m_webView == [lastView nextValidKeyView])
            return;
        [[m_webView window] selectKeyViewFollowingView:lastView];
    } else {
        // avoid triggering assertions if the WebView is the only thing in the key loop
        if ([m_webView _becomingFirstResponderFromOutside] && m_webView == [m_webView previousValidKeyView])
            return;
        [[m_webView window] selectKeyViewPrecedingView:m_webView];
    }
}

void WebChromeClient::focusedNodeChanged(Node*)
{
}

Page* WebChromeClient::createWindow(Frame* frame, const FrameLoadRequest& request, const WindowFeatures& features)
{
    NSURLRequest *URLRequest = nil;
    if (!request.isEmpty())
        URLRequest = request.resourceRequest().nsURLRequest();
    
    id delegate = [m_webView UIDelegate];
    WebView *newWebView;
    
    if ([delegate respondsToSelector:@selector(webView:createWebViewWithRequest:windowFeatures:)]) {
        NSNumber *x = features.xSet ? [[NSNumber alloc] initWithFloat:features.x] : nil;
        NSNumber *y = features.ySet ? [[NSNumber alloc] initWithFloat:features.y] : nil;
        NSNumber *width = features.widthSet ? [[NSNumber alloc] initWithFloat:features.width] : nil;
        NSNumber *height = features.heightSet ? [[NSNumber alloc] initWithFloat:features.height] : nil;
        NSNumber *menuBarVisible = [[NSNumber alloc] initWithBool:features.menuBarVisible];
        NSNumber *statusBarVisible = [[NSNumber alloc] initWithBool:features.statusBarVisible];
        NSNumber *toolBarVisible = [[NSNumber alloc] initWithBool:features.toolBarVisible];
        NSNumber *scrollbarsVisible = [[NSNumber alloc] initWithBool:features.scrollbarsVisible];
        NSNumber *resizable = [[NSNumber alloc] initWithBool:features.resizable];
        NSNumber *fullscreen = [[NSNumber alloc] initWithBool:features.fullscreen];
        NSNumber *dialog = [[NSNumber alloc] initWithBool:features.dialog];
        
        NSMutableDictionary *dictFeatures = [[NSMutableDictionary alloc] initWithObjectsAndKeys:
                                             menuBarVisible, @"menuBarVisible", 
                                             statusBarVisible, @"statusBarVisible",
                                             toolBarVisible, @"toolBarVisible",
                                             scrollbarsVisible, @"scrollbarsVisible",
                                             resizable, @"resizable",
                                             fullscreen, @"fullscreen",
                                             dialog, @"dialog",
                                             nil];
        
        if (x)
            [dictFeatures setObject:x forKey:@"x"];
        if (y)
            [dictFeatures setObject:y forKey:@"y"];
        if (width)
            [dictFeatures setObject:width forKey:@"width"];
        if (height)
            [dictFeatures setObject:height forKey:@"height"];
        
        newWebView = CallUIDelegate(m_webView, @selector(webView:createWebViewWithRequest:windowFeatures:), URLRequest, dictFeatures);
        
        [dictFeatures release];
        [x release];
        [y release];
        [width release];
        [height release];
        [menuBarVisible release];
        [statusBarVisible release];
        [toolBarVisible release];
        [scrollbarsVisible release];
        [resizable release];
        [fullscreen release];
        [dialog release];
    } else if (features.dialog && [delegate respondsToSelector:@selector(webView:createWebViewModalDialogWithRequest:)]) {
        newWebView = CallUIDelegate(m_webView, @selector(webView:createWebViewModalDialogWithRequest:), URLRequest);
    } else {
        newWebView = CallUIDelegate(m_webView, @selector(webView:createWebViewWithRequest:), URLRequest);
    }

#if USE(PLUGIN_HOST_PROCESS)
    if (newWebView)
        WebKit::NetscapePluginHostManager::shared().didCreateWindow();
#endif
    
    return core(newWebView);
}

void WebChromeClient::show()
{
    [[m_webView _UIDelegateForwarder] webViewShow:m_webView];
}

bool WebChromeClient::canRunModal()
{
    return [[m_webView UIDelegate] respondsToSelector:@selector(webViewRunModal:)];
}

void WebChromeClient::runModal()
{
    CallUIDelegate(m_webView, @selector(webViewRunModal:));
}

void WebChromeClient::setToolbarsVisible(bool b)
{
    [[m_webView _UIDelegateForwarder] webView:m_webView setToolbarsVisible:b];
}

bool WebChromeClient::toolbarsVisible()
{
    return CallUIDelegateReturningBoolean(NO, m_webView, @selector(webViewAreToolbarsVisible:));
}

void WebChromeClient::setStatusbarVisible(bool b)
{
    [[m_webView _UIDelegateForwarder] webView:m_webView setStatusBarVisible:b];
}

bool WebChromeClient::statusbarVisible()
{
    return CallUIDelegateReturningBoolean(NO, m_webView, @selector(webViewIsStatusBarVisible:));
}

void WebChromeClient::setScrollbarsVisible(bool b)
{
    [[[m_webView mainFrame] frameView] setAllowsScrolling:b];
}

bool WebChromeClient::scrollbarsVisible()
{
    return [[[m_webView mainFrame] frameView] allowsScrolling];
}

void WebChromeClient::setMenubarVisible(bool)
{
    // The menubar is always visible in Mac OS X.
    return;
}

bool WebChromeClient::menubarVisible()
{
    // The menubar is always visible in Mac OS X.
    return true;
}

void WebChromeClient::setResizable(bool b)
{
    [[m_webView _UIDelegateForwarder] webView:m_webView setResizable:b];
}

void WebChromeClient::addMessageToConsole(MessageSource source, MessageType type, MessageLevel level, const String& message, unsigned int lineNumber, const String& sourceURL)
{
    id delegate = [m_webView UIDelegate];
    SEL selector = @selector(webView:addMessageToConsole:);
    if (![delegate respondsToSelector:selector])
        return;

    NSDictionary *dictionary = [[NSDictionary alloc] initWithObjectsAndKeys:
        (NSString *)message, @"message", [NSNumber numberWithUnsignedInt:lineNumber], @"lineNumber",
        (NSString *)sourceURL, @"sourceURL", NULL];

    CallUIDelegate(m_webView, selector, dictionary);

    [dictionary release];
}

bool WebChromeClient::canRunBeforeUnloadConfirmPanel()
{
    return [[m_webView UIDelegate] respondsToSelector:@selector(webView:runBeforeUnloadConfirmPanelWithMessage:initiatedByFrame:)];
}

bool WebChromeClient::runBeforeUnloadConfirmPanel(const String& message, Frame* frame)
{
    return CallUIDelegateReturningBoolean(true, m_webView, @selector(webView:runBeforeUnloadConfirmPanelWithMessage:initiatedByFrame:), message, kit(frame));
}

void WebChromeClient::closeWindowSoon()
{
    // We need to remove the parent WebView from WebViewSets here, before it actually
    // closes, to make sure that JavaScript code that executes before it closes
    // can't find it. Otherwise, window.open will select a closed WebView instead of 
    // opening a new one <rdar://problem/3572585>.

    // We also need to stop the load to prevent further parsing or JavaScript execution
    // after the window has torn down <rdar://problem/4161660>.
  
    // FIXME: This code assumes that the UI delegate will respond to a webViewClose
    // message by actually closing the WebView. Safari guarantees this behavior, but other apps might not.
    // This approach is an inherent limitation of not making a close execute immediately
    // after a call to window.close.

    [m_webView setGroupName:nil];
    [m_webView stopLoading:nil];
    [m_webView performSelector:@selector(_closeWindow) withObject:nil afterDelay:0.0];
}

void WebChromeClient::runJavaScriptAlert(Frame* frame, const String& message)
{
    id delegate = [m_webView UIDelegate];
    SEL selector = @selector(webView:runJavaScriptAlertPanelWithMessage:initiatedByFrame:);
    if ([delegate respondsToSelector:selector]) {
        CallUIDelegate(m_webView, selector, message, kit(frame));
        return;
    }

    // Call the old version of the delegate method if it is implemented.
    selector = @selector(webView:runJavaScriptAlertPanelWithMessage:);
    if ([delegate respondsToSelector:selector]) {
        CallUIDelegate(m_webView, selector, message);
        return;
    }
}

bool WebChromeClient::runJavaScriptConfirm(Frame* frame, const String& message)
{
    id delegate = [m_webView UIDelegate];
    SEL selector = @selector(webView:runJavaScriptConfirmPanelWithMessage:initiatedByFrame:);
    if ([delegate respondsToSelector:selector])
        return CallUIDelegateReturningBoolean(NO, m_webView, selector, message, kit(frame));

    // Call the old version of the delegate method if it is implemented.
    selector = @selector(webView:runJavaScriptConfirmPanelWithMessage:);
    if ([delegate respondsToSelector:selector])
        return CallUIDelegateReturningBoolean(NO, m_webView, selector, message);

    return NO;
}

bool WebChromeClient::runJavaScriptPrompt(Frame* frame, const String& prompt, const String& defaultText, String& result)
{
    id delegate = [m_webView UIDelegate];
    SEL selector = @selector(webView:runJavaScriptTextInputPanelWithPrompt:defaultText:initiatedByFrame:);
    if ([delegate respondsToSelector:selector]) {
        result = (NSString *)CallUIDelegate(m_webView, selector, prompt, defaultText, kit(frame));
        return !result.isNull();
    }

    // Call the old version of the delegate method if it is implemented.
    selector = @selector(webView:runJavaScriptTextInputPanelWithPrompt:defaultText:);
    if ([delegate respondsToSelector:selector]) {
        result = (NSString *)CallUIDelegate(m_webView, selector, prompt, defaultText);
        return !result.isNull();
    }

    result = [[WebDefaultUIDelegate sharedUIDelegate] webView:m_webView runJavaScriptTextInputPanelWithPrompt:prompt defaultText:defaultText initiatedByFrame:kit(frame)];
    return !result.isNull();
}

bool WebChromeClient::shouldInterruptJavaScript()
{
    return CallUIDelegate(m_webView, @selector(webViewShouldInterruptJavaScript:));
}

void WebChromeClient::setStatusbarText(const String& status)
{
    // We want the temporaries allocated here to be released even before returning to the 
    // event loop; see <http://bugs.webkit.org/show_bug.cgi?id=9880>.
    NSAutoreleasePool* localPool = [[NSAutoreleasePool alloc] init];
    CallUIDelegate(m_webView, @selector(webView:setStatusText:), (NSString *)status);
    [localPool drain];
}

bool WebChromeClient::tabsToLinks() const
{
    return [[m_webView preferences] tabsToLinks];
}

IntRect WebChromeClient::windowResizerRect() const
{
    NSRect rect = [[m_webView window] _growBoxRect];
    if ([m_webView _usesDocumentViews])
        return enclosingIntRect(rect);
    return enclosingIntRect([m_webView convertRect:rect fromView:nil]);
}

void WebChromeClient::repaint(const IntRect& rect, bool contentChanged, bool immediate, bool repaintContentOnly)
{
    if ([m_webView _usesDocumentViews])
        return;
    
    if (contentChanged)
        [m_webView setNeedsDisplayInRect:rect];
    
    if (immediate) {
        [[m_webView window] displayIfNeeded];
        [[m_webView window] flushWindowIfNeeded];
    }
}

void WebChromeClient::scroll(const IntSize&, const IntRect&, const IntRect&)
{
}

IntPoint WebChromeClient::screenToWindow(const IntPoint& p) const
{
    if ([m_webView _usesDocumentViews])
        return p;
    NSPoint windowCoord = [[m_webView window] convertScreenToBase:p];
    return IntPoint([m_webView convertPoint:windowCoord fromView:nil]);
}

IntRect WebChromeClient::windowToScreen(const IntRect& r) const
{
    if ([m_webView _usesDocumentViews])
        return r;
    NSRect tempRect = r;
    tempRect = [m_webView convertRect:tempRect toView:nil];
    tempRect.origin = [[m_webView window] convertBaseToScreen:tempRect.origin];
    return enclosingIntRect(tempRect);
}

PlatformPageClient WebChromeClient::platformPageClient() const
{
    if ([m_webView _usesDocumentViews])
        return 0;
    return m_webView;
}

void WebChromeClient::contentsSizeChanged(Frame*, const IntSize&) const
{
}

void WebChromeClient::scrollRectIntoView(const IntRect& r, const ScrollView* scrollView) const
{
    // FIXME: This scrolling behavior should be under the control of the embedding client (rather than something
    // we just do ourselves).
    
    IntRect scrollRect = r;
    NSView *startView = m_webView;
    if ([m_webView _usesDocumentViews]) {
        // We have to convert back to document view coordinates.
        // It doesn't make sense for the scrollRectIntoView API to take document view coordinates.
        scrollRect.move(scrollView->scrollOffset());
        startView = [[[m_webView mainFrame] frameView] documentView];
    }
    NSRect rect = scrollRect;
    for (NSView *view = startView; view; view = [view superview]) { 
        if ([view isKindOfClass:[NSClipView class]]) { 
            NSClipView *clipView = (NSClipView *)view; 
            NSView *documentView = [clipView documentView]; 
            [documentView scrollRectToVisible:[documentView convertRect:rect fromView:startView]]; 
        } 
    }
}

// End host window methods.

void WebChromeClient::mouseDidMoveOverElement(const HitTestResult& result, unsigned modifierFlags)
{
    WebElementDictionary *element = [[WebElementDictionary alloc] initWithHitTestResult:result];
    [m_webView _mouseDidMoveOverElement:element modifierFlags:modifierFlags];
    [element release];
}

void WebChromeClient::setToolTip(const String& toolTip, TextDirection)
{
    [m_webView _setToolTip:toolTip];
}

void WebChromeClient::print(Frame* frame)
{
    WebFrame *webFrame = kit(frame);
    if ([[m_webView UIDelegate] respondsToSelector:@selector(webView:printFrame:)])
        CallUIDelegate(m_webView, @selector(webView:printFrame:), webFrame);
    else if ([m_webView _usesDocumentViews])
        CallUIDelegate(m_webView, @selector(webView:printFrameView:), [webFrame frameView]);
}

#if ENABLE(DATABASE)
void WebChromeClient::exceededDatabaseQuota(Frame* frame, const String& databaseName)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    WebSecurityOrigin *webOrigin = [[WebSecurityOrigin alloc] _initWithWebCoreSecurityOrigin:frame->document()->securityOrigin()];
    // FIXME: remove this workaround once shipping Safari has the necessary delegate implemented.
    if (WKAppVersionCheckLessThan(@"com.apple.Safari", -1, 3.1)) {
        const unsigned long long defaultQuota = 5 * 1024 * 1024; // 5 megabytes should hopefully be enough to test storage support.
        [webOrigin setQuota:defaultQuota];
    } else
        CallUIDelegate(m_webView, @selector(webView:frame:exceededDatabaseQuotaForSecurityOrigin:database:), kit(frame), webOrigin, (NSString *)databaseName);
    [webOrigin release];

    END_BLOCK_OBJC_EXCEPTIONS;
}
#endif

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
void WebChromeClient::reachedMaxAppCacheSize(int64_t spaceNeeded)
{
    // FIXME: Free some space.
}
#endif
    
void WebChromeClient::populateVisitedLinks()
{
    if ([m_webView historyDelegate]) {
        WebHistoryDelegateImplementationCache* implementations = WebViewGetHistoryDelegateImplementations(m_webView);
        
        if (implementations->populateVisitedLinksFunc)
            CallHistoryDelegate(implementations->populateVisitedLinksFunc, m_webView, @selector(populateVisitedLinksForWebView:));

        return;
    }

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [[WebHistory optionalSharedHistory] _addVisitedLinksToPageGroup:[m_webView page]->group()];
    END_BLOCK_OBJC_EXCEPTIONS;
}

#if ENABLE(DASHBOARD_SUPPORT)
void WebChromeClient::dashboardRegionsChanged()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSMutableDictionary *regions = core([m_webView mainFrame])->dashboardRegionsDictionary();
    [m_webView _addScrollerDashboardRegions:regions];

    CallUIDelegate(m_webView, @selector(webView:dashboardRegionsChanged:), regions);

    END_BLOCK_OBJC_EXCEPTIONS;
}
#endif

FloatRect WebChromeClient::customHighlightRect(Node* node, const AtomicString& type, const FloatRect& lineRect)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSView *documentView = [[kit(node->document()->frame()) frameView] documentView];
    if (![documentView isKindOfClass:[WebHTMLView class]])
        return NSZeroRect;

    WebHTMLView *webHTMLView = (WebHTMLView *)documentView;
    id<WebHTMLHighlighter> highlighter = [webHTMLView _highlighterForType:type];
    if ([(NSObject *)highlighter respondsToSelector:@selector(highlightRectForLine:representedNode:)])
        return [highlighter highlightRectForLine:lineRect representedNode:kit(node)];
    return [highlighter highlightRectForLine:lineRect];

    END_BLOCK_OBJC_EXCEPTIONS;

    return NSZeroRect;
}

void WebChromeClient::paintCustomHighlight(Node* node, const AtomicString& type, const FloatRect& boxRect, const FloatRect& lineRect,
    bool behindText, bool entireLine)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSView *documentView = [[kit(node->document()->frame()) frameView] documentView];
    if (![documentView isKindOfClass:[WebHTMLView class]])
        return;

    WebHTMLView *webHTMLView = (WebHTMLView *)documentView;
    id<WebHTMLHighlighter> highlighter = [webHTMLView _highlighterForType:type];
    if ([(NSObject *)highlighter respondsToSelector:@selector(paintHighlightForBox:onLine:behindText:entireLine:representedNode:)])
        [highlighter paintHighlightForBox:boxRect onLine:lineRect behindText:behindText entireLine:entireLine representedNode:kit(node)];
    else
        [highlighter paintHighlightForBox:boxRect onLine:lineRect behindText:behindText entireLine:entireLine];

    END_BLOCK_OBJC_EXCEPTIONS;
}

void WebChromeClient::runOpenPanel(Frame*, PassRefPtr<FileChooser> chooser)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    BOOL allowMultipleFiles = chooser->allowsMultipleFiles();
    WebOpenPanelResultListener *listener = [[WebOpenPanelResultListener alloc] initWithChooser:chooser];
    id delegate = [m_webView UIDelegate];
    if ([delegate respondsToSelector:@selector(webView:runOpenPanelForFileButtonWithResultListener:allowMultipleFiles:)])
        CallUIDelegate(m_webView, @selector(webView:runOpenPanelForFileButtonWithResultListener:allowMultipleFiles:), listener, allowMultipleFiles);
    else
        CallUIDelegate(m_webView, @selector(webView:runOpenPanelForFileButtonWithResultListener:), listener);
    [listener release];
    END_BLOCK_OBJC_EXCEPTIONS;
}

KeyboardUIMode WebChromeClient::keyboardUIMode()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [m_webView _keyboardUIMode];
    END_BLOCK_OBJC_EXCEPTIONS;
    return KeyboardAccessDefault;
}

NSResponder *WebChromeClient::firstResponder()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[m_webView _UIDelegateForwarder] webViewFirstResponder:m_webView];
    END_BLOCK_OBJC_EXCEPTIONS;
    return nil;
}

void WebChromeClient::makeFirstResponder(NSResponder *responder)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [m_webView _pushPerformingProgrammaticFocus];
    [[m_webView _UIDelegateForwarder] webView:m_webView makeFirstResponder:responder];
    [m_webView _popPerformingProgrammaticFocus];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void WebChromeClient::willPopUpMenu(NSMenu *menu)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    CallUIDelegate(m_webView, @selector(webView:willPopupMenu:), menu);
    END_BLOCK_OBJC_EXCEPTIONS;
}

bool WebChromeClient::shouldReplaceWithGeneratedFileForUpload(const String& path, String& generatedFilename)
{
    NSString* filename;
    if (![[m_webView _UIDelegateForwarder] webView:m_webView shouldReplaceUploadFile:path usingGeneratedFilename:&filename])
        return false;
    generatedFilename = filename;
    return true;
}

String WebChromeClient::generateReplacementFile(const String& path)
{
    return [[m_webView _UIDelegateForwarder] webView:m_webView generateReplacementFile:path];
}

void WebChromeClient::formStateDidChange(const WebCore::Node* node)
{
    CallUIDelegate(m_webView, @selector(webView:formStateDidChangeForNode:), kit(const_cast<WebCore::Node*>(node)));
}

void WebChromeClient::formDidFocus(const WebCore::Node* node)
{
    CallUIDelegate(m_webView, @selector(webView:formDidFocusNode:), kit(const_cast<WebCore::Node*>(node)));
}

void WebChromeClient::formDidBlur(const WebCore::Node* node)
{
    CallUIDelegate(m_webView, @selector(webView:formDidBlurNode:), kit(const_cast<WebCore::Node*>(node)));
}

#if USE(ACCELERATED_COMPOSITING)

void WebChromeClient::attachRootGraphicsLayer(Frame* frame, GraphicsLayer* graphicsLayer)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSView *documentView = [[kit(frame) frameView] documentView];
    if (![documentView isKindOfClass:[WebHTMLView class]]) {
        // We should never be attaching when we don't have a WebHTMLView.
        ASSERT(!graphicsLayer);
        return;
    }

    WebHTMLView *webHTMLView = (WebHTMLView *)documentView;
    if (graphicsLayer)
        [webHTMLView attachRootLayer:graphicsLayer->nativeLayer()];
    else
        [webHTMLView detachRootLayer];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void WebChromeClient::setNeedsOneShotDrawingSynchronization()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [m_webView _setNeedsOneShotDrawingSynchronization:YES];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void WebChromeClient::scheduleCompositingLayerSync()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [m_webView _scheduleCompositingLayerSync];
    END_BLOCK_OBJC_EXCEPTIONS;
}

#endif

#if ENABLE(VIDEO)

bool WebChromeClient::supportsFullscreenForNode(const Node* node)
{
    return node->hasTagName(WebCore::HTMLNames::videoTag);
}

void WebChromeClient::enterFullscreenForNode(Node* node)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [m_webView _enterFullscreenForNode:node];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void WebChromeClient::exitFullscreenForNode(Node*)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [m_webView _exitFullscreen];
    END_BLOCK_OBJC_EXCEPTIONS;    
}

#endif

void WebChromeClient::requestGeolocationPermissionForFrame(Frame* frame, Geolocation* geolocation)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    WebSecurityOrigin *webOrigin = [[WebSecurityOrigin alloc] _initWithWebCoreSecurityOrigin:frame->document()->securityOrigin()];
    WebGeolocation *webGeolocation = [[WebGeolocation alloc] _initWithWebCoreGeolocation:geolocation];
    CallUIDelegate(m_webView, @selector(webView:frame:requestGeolocationPermission:securityOrigin:), kit(frame), webGeolocation, webOrigin);
    [webOrigin release];
    [webGeolocation release];

    END_BLOCK_OBJC_EXCEPTIONS;
}

@implementation WebOpenPanelResultListener

- (id)initWithChooser:(PassRefPtr<FileChooser>)chooser
{
    self = [super init];
    if (!self)
        return nil;
    _chooser = chooser.releaseRef();
    return self;
}

#ifndef NDEBUG

- (void)dealloc
{
    ASSERT(!_chooser);
    [super dealloc];
}

- (void)finalize
{
    ASSERT(!_chooser);
    [super finalize];
}

#endif

- (void)cancel
{
    ASSERT(_chooser);
    if (!_chooser)
        return;
    _chooser->deref();
    _chooser = 0;
}

- (void)chooseFilename:(NSString *)filename
{
    ASSERT(_chooser);
    if (!_chooser)
        return;
    _chooser->chooseFile(filename);
    _chooser->deref();
    _chooser = 0;
}

- (void)chooseFilenames:(NSArray *)filenames
{
    ASSERT(_chooser);
    if (!_chooser)
        return;
    int count = [filenames count]; 
    Vector<String> names(count);
    for (int i = 0; i < count; i++)
        names[i] = [filenames objectAtIndex:i];
    _chooser->chooseFiles(names);
    _chooser->deref();
    _chooser = 0;
}

@end
