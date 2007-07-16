/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Trolltech ASA
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

#import "WebDefaultUIDelegate.h"
#import "WebElementDictionary.h"
#import "WebFrameInternal.h"
#import "WebFrameView.h"
#import "WebHTMLView.h"
#import "WebHTMLViewPrivate.h"
#import "WebNSURLRequestExtras.h"
#import "WebUIDelegate.h"
#import "WebUIDelegatePrivate.h"
#import "WebView.h"
#import "WebViewInternal.h"
#import <WebCore/BlockExceptions.h>
#import <WebCore/FloatRect.h>
#import <WebCore/FrameLoadRequest.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/IntRect.h>
#import <WebCore/PlatformScreen.h>
#import <WebCore/PlatformString.h>
#import <WebCore/ResourceRequest.h>
#import <wtf/PassRefPtr.h>

@interface NSView (AppKitSecretsWebBridgeKnowsAbout)
- (NSView *)_findLastViewInKeyViewLoop;
@end

using namespace WebCore;

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

Page* WebChromeClient::createWindow(Frame*, const FrameLoadRequest& request)
{
    NSURLRequest *URLRequest = nil;
    if (!request.isEmpty())
        URLRequest = request.resourceRequest().nsURLRequest();

    WebView *newWebView;
    id delegate = [m_webView UIDelegate];
    if ([delegate respondsToSelector:@selector(webView:createWebViewWithRequest:)])
        newWebView = [delegate webView:m_webView createWebViewWithRequest:URLRequest];
    else
        newWebView = [[WebDefaultUIDelegate sharedUIDelegate] webView:m_webView createWebViewWithRequest:URLRequest];

    return core(newWebView);
}

Page* WebChromeClient::createModalDialog(Frame*, const FrameLoadRequest& request)
{
    NSURLRequest *URLRequest = nil;
    if (!request.isEmpty())
        URLRequest = request.resourceRequest().nsURLRequest();

    WebView *newWebView = nil;
    id delegate = [m_webView UIDelegate];
    if ([delegate respondsToSelector:@selector(webView:createWebViewModalDialogWithRequest:)])
        newWebView = [delegate webView:m_webView createWebViewModalDialogWithRequest:URLRequest];
    else if ([delegate respondsToSelector:@selector(webView:createWebViewWithRequest:)])
        newWebView = [delegate webView:m_webView createWebViewWithRequest:URLRequest];
    else
        newWebView = [[WebDefaultUIDelegate sharedUIDelegate] webView:m_webView createWebViewWithRequest:URLRequest];

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
    [[m_webView UIDelegate] webViewRunModal:m_webView];
}

void WebChromeClient::setToolbarsVisible(bool b)
{
    [[m_webView _UIDelegateForwarder] webView:m_webView setToolbarsVisible:b];
}

bool WebChromeClient::toolbarsVisible()
{
    id delegate = [m_webView UIDelegate];
    if ([delegate respondsToSelector:@selector(webViewAreToolbarsVisible:)])
        return [delegate webViewAreToolbarsVisible:m_webView];
    return [[WebDefaultUIDelegate sharedUIDelegate] webViewAreToolbarsVisible:m_webView];
}

void WebChromeClient::setStatusbarVisible(bool b)
{
    [[m_webView _UIDelegateForwarder] webView:m_webView setStatusBarVisible:b];
}

bool WebChromeClient::statusbarVisible()
{
    id delegate = [m_webView UIDelegate];
    if ([delegate respondsToSelector:@selector(webViewIsStatusBarVisible:)])
        return [delegate webViewIsStatusBarVisible:m_webView];
    return [[WebDefaultUIDelegate sharedUIDelegate] webViewIsStatusBarVisible:m_webView];
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

void WebChromeClient::addMessageToConsole(const String& message, unsigned int lineNumber, const String& sourceURL)
{
    id wd = [m_webView UIDelegate];
    if ([wd respondsToSelector:@selector(webView:addMessageToConsole:)]) {
        NSDictionary *dictionary = [NSDictionary dictionaryWithObjectsAndKeys:
            (NSString *)message, @"message",
            [NSNumber numberWithInt: lineNumber], @"lineNumber",
            (NSString *)sourceURL, @"sourceURL",
            NULL];
        
        [wd webView:m_webView addMessageToConsole:dictionary];
    }    
}

bool WebChromeClient::canRunBeforeUnloadConfirmPanel()
{
    id wd = [m_webView UIDelegate];
    return [wd respondsToSelector:@selector(webView:runBeforeUnloadConfirmPanelWithMessage:initiatedByFrame:)];
}

bool WebChromeClient::runBeforeUnloadConfirmPanel(const String& message, Frame* frame)
{
    id wd = [m_webView UIDelegate];
    if ([wd respondsToSelector:@selector(webView:runBeforeUnloadConfirmPanelWithMessage:initiatedByFrame:)])
        return [wd webView:m_webView runBeforeUnloadConfirmPanelWithMessage:message initiatedByFrame:kit(frame)];
    return true;
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
    id wd = [m_webView UIDelegate];
    // Check whether delegate implements new version, then whether delegate implements old version. If neither,
    // fall back to shared delegate's implementation of new version.
    if ([wd respondsToSelector:@selector(webView:runJavaScriptAlertPanelWithMessage:initiatedByFrame:)])
        [wd webView:m_webView runJavaScriptAlertPanelWithMessage:message initiatedByFrame:kit(frame)];
    else if ([wd respondsToSelector:@selector(webView:runJavaScriptAlertPanelWithMessage:)])
        [wd webView:m_webView runJavaScriptAlertPanelWithMessage:message];
    else
        [[WebDefaultUIDelegate sharedUIDelegate] webView:m_webView runJavaScriptAlertPanelWithMessage:message initiatedByFrame:kit(frame)];    
}

bool WebChromeClient::runJavaScriptConfirm(Frame* frame, const String& message)
{
    id wd = [m_webView UIDelegate];
    // Check whether delegate implements new version, then whether delegate implements old version. If neither,
    // fall back to shared delegate's implementation of new version.
    if ([wd respondsToSelector:@selector(webView:runJavaScriptConfirmPanelWithMessage:initiatedByFrame:)])
        return [wd webView:m_webView runJavaScriptConfirmPanelWithMessage:message initiatedByFrame:kit(frame)];
    if ([wd respondsToSelector:@selector(webView:runJavaScriptConfirmPanelWithMessage:)])
        return [wd webView:m_webView runJavaScriptConfirmPanelWithMessage:message];    
    return [[WebDefaultUIDelegate sharedUIDelegate] webView:m_webView runJavaScriptConfirmPanelWithMessage:message initiatedByFrame:kit(frame)];
}

bool WebChromeClient::runJavaScriptPrompt(Frame* frame, const String& prompt, const String& defaultText, String& result)
{
    id wd = [m_webView UIDelegate];
    // Check whether delegate implements new version, then whether delegate implements old version. If neither,
    // fall back to shared delegate's implementation of new version.
    if ([wd respondsToSelector:@selector(webView:runJavaScriptTextInputPanelWithPrompt:defaultText:initiatedByFrame:)])
        result = [wd webView:m_webView runJavaScriptTextInputPanelWithPrompt:prompt defaultText:defaultText initiatedByFrame:kit(frame)];
    else if ([wd respondsToSelector:@selector(webView:runJavaScriptTextInputPanelWithPrompt:defaultText:)])
        result = [wd webView:m_webView runJavaScriptTextInputPanelWithPrompt:prompt defaultText:defaultText];
    else
        result = [[WebDefaultUIDelegate sharedUIDelegate] webView:m_webView runJavaScriptTextInputPanelWithPrompt:prompt defaultText:defaultText initiatedByFrame:kit(frame)];
    
    return !result.isNull();
}

bool WebChromeClient::shouldInterruptJavaScript()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    id wd = [m_webView UIDelegate];
    if ([wd respondsToSelector:@selector(webViewShouldInterruptJavaScript:)])
        return [wd webViewShouldInterruptJavaScript:m_webView];
    return false;
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return false;
}

void WebChromeClient::setStatusbarText(const WebCore::String& status)
{
    id wd = [m_webView UIDelegate];

    if ([wd respondsToSelector:@selector(webView:setStatusText:)]) {
        // We want the temporaries allocated here to be released even before returning to the 
        // event loop; see <http://bugs.webkit.org/show_bug.cgi?id=9880>.
        NSAutoreleasePool* localPool = [[NSAutoreleasePool alloc] init];
    
        [wd webView:m_webView setStatusText:status];
        
        [localPool drain];
    }
}

bool WebChromeClient::tabsToLinks() const
{
    return [[m_webView preferences] tabsToLinks];
}

IntRect WebChromeClient::windowResizerRect() const
{
    return IntRect();
}

void WebChromeClient::addToDirtyRegion(const IntRect&)
{
}

void WebChromeClient::scrollBackingStore(int, int, const IntRect&, const IntRect&)
{
}

void WebChromeClient::updateBackingStore()
{
}

void WebChromeClient::mouseDidMoveOverElement(const HitTestResult& result, unsigned modifierFlags)
{
    WebElementDictionary *element = [[WebElementDictionary alloc] initWithHitTestResult:result];
    [m_webView _mouseDidMoveOverElement:element modifierFlags:modifierFlags];
    [element release];
}

void WebChromeClient::setToolTip(const String& toolTip)
{
    [(WebHTMLView *)[[[m_webView mainFrame] frameView] documentView] _setToolTip:toolTip];
}

void WebChromeClient::print(Frame* frame)
{
    WebFrameView* frameView = [kit(frame) frameView];
    id wd = [m_webView UIDelegate];
    if ([wd respondsToSelector:@selector(webView:printFrameView:)])
        [wd webView:m_webView printFrameView:frameView];
    else
        [[WebDefaultUIDelegate sharedUIDelegate] webView:m_webView printFrameView:frameView];
}
