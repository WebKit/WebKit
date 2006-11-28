/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#import "WebFrameInternal.h"
#import "WebFrameView.h"
#import "WebNSURLRequestExtras.h"
#import "WebUIDelegate.h"
#import "WebUIDelegatePrivate.h"
#import "WebView.h"
#import "WebViewInternal.h"
#import <WebCore/FloatRect.h>
#import <WebCore/FrameLoadRequest.h>
#import <WebCore/PlatformString.h>
#import <WebCore/ResourceRequestMac.h>
#import <WebCore/Screen.h>
#import <wtf/PassRefPtr.h>

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
    return [[m_webView window] userSpaceScaleFactor];
}

void WebChromeClient::focus()
{
    [[m_webView _UIDelegateForwarder] webViewFocus:m_webView];
}

void WebChromeClient::unfocus()
{
    [[m_webView _UIDelegateForwarder] webViewUnfocus:m_webView];
}

Page* WebChromeClient::createWindow(const FrameLoadRequest& request)
{
    NSURLRequest *URLRequest = nil;
    if (!request.isEmpty())
        URLRequest = nsURLRequest(request.resourceRequest());

    WebView *newWebView;
    id delegate = [m_webView UIDelegate];
    if ([delegate respondsToSelector:@selector(webView:createWebViewWithRequest:)])
        newWebView = [delegate webView:m_webView createWebViewWithRequest:URLRequest];
    else
        newWebView = [[WebDefaultUIDelegate sharedUIDelegate] webView:m_webView createWebViewWithRequest:URLRequest];

    return core(newWebView);
}

Page* WebChromeClient::createModalDialog(const FrameLoadRequest& request)
{
    NSURLRequest *URLRequest = nil;
    if (!request.isEmpty())
        URLRequest = nsURLRequest(request.resourceRequest());

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


