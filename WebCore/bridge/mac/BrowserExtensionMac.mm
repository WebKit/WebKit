/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "BrowserExtensionMac.h"

#import "KWQExceptions.h"
#import "MacFrame.h"
#import "WebCoreFrameBridge.h"
#import <kxmlcore/Assertions.h>
#import "FrameTree.h"

namespace WebCore {

BrowserExtensionMac::BrowserExtensionMac(Frame *frame)
    : m_frame(Mac(frame))
{
}

void BrowserExtensionMac::openURLRequest(const KURL& url, const URLArgs& args)
{
    if (url.protocol().lower() == "javascript") {
	m_frame->createEmptyDocument();
	m_frame->replaceContentsWithScriptResult(url);
     } else {
	m_frame->openURLRequest(url, args);
    }
}

void BrowserExtensionMac::openURLNotify()
{
}

void BrowserExtensionMac::createNewWindow(const KURL &url, const URLArgs &urlArgs) 
{
    createNewWindow(url, urlArgs, WindowArgs(), NULL);
}

void BrowserExtensionMac::createNewWindow(const KURL &url, 
						const URLArgs &urlArgs, 
						const WindowArgs &winArgs, 
						Frame*& part)
{
    createNewWindow(url, urlArgs, winArgs, &part);
}

void BrowserExtensionMac::createNewWindow(const KURL &url, 
						const URLArgs &urlArgs, 
						const WindowArgs &winArgs, 
						Frame** partResult)
{ 
    KWQ_BLOCK_EXCEPTIONS;

    ASSERT(!winArgs.dialog || urlArgs.frameName.isEmpty());

    if (partResult)
	*partResult = NULL;

    NSString *frameName = urlArgs.frameName.length() == 0 ? nil : urlArgs.frameName.getNSString();
    if (frameName) {
        // FIXME: Can't we just use m_frame->findFrame?
        if (WebCoreFrameBridge *bridge = [m_frame->bridge() findFrameNamed:frameName]) {
            if (!url.isEmpty()) {
                DOMString argsReferrer = urlArgs.metaData().get("referrer");
                NSString *referrer;
                if (!argsReferrer.isEmpty())
                    referrer = argsReferrer;
                else
                    referrer = [m_frame->bridge() referrer];

                [bridge loadURL:url.getNSURL() 
                       referrer:referrer 
                         reload:urlArgs.reload 
                    userGesture:true 
                         target:nil 
                triggeringEvent:nil 
                           form:nil 
                     formValues:nil];
            }

            [bridge focusWindow];

            if (partResult)
                *partResult = [bridge impl];

            return;
        }
    }
    
    WebCoreFrameBridge *bridge;
    if (winArgs.dialog)
        bridge = [m_frame->bridge() createModalDialogWithURL:url.getNSURL()];
    else
        bridge = [m_frame->bridge() createWindowWithURL:url.getNSURL() frameName:frameName];

    if (!bridge)
        return;
    
    if ([bridge impl])
	[bridge impl]->tree()->setName(urlArgs.frameName);
    
    if (partResult)
	*partResult = [bridge impl];
    
    [bridge setToolbarsVisible:winArgs.toolBarVisible || winArgs.locationBarVisible];
    [bridge setStatusbarVisible:winArgs.statusBarVisible];
    [bridge setScrollbarsVisible:winArgs.scrollBarsVisible];
    [bridge setWindowIsResizable:winArgs.resizable];
    
    NSRect windowFrame = [bridge windowFrame];

    NSSize size = { 1, 1 }; // workaround for 4213314
    NSSize scaleRect = [[bridge webView] convertSize:size toView:nil];
    float scaleX = scaleRect.width;
    float scaleY = scaleRect.height;

    // In JavaScript, the coordinate system of the window is the same as the coordinate system
    // of the WebView, so we translate 'left' and 'top' from WebView coordinates to window coordinates.
    // (If the screen resolution is scaled, window coordinates won't match WebView coordinates.)
    if (winArgs.xSet)
	windowFrame.origin.x = winArgs.x * scaleX;
    if (winArgs.ySet) {
	// In JavaScript, (0, 0) is the top left corner of the screen.
	float screenTop = NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]);
	windowFrame.origin.y = screenTop - (winArgs.y * scaleY) - windowFrame.size.height;
    }

    // 'width' and 'height' specify the dimensions of the WebView, but we can only resize the window, 
    // so we compute a delta, translate it to window coordinates, and use the translated delta 
    // to resize the window.
    NSRect webViewFrame = [[bridge webView] frame];
    if (winArgs.widthSet) {
	float widthDelta = (winArgs.width - webViewFrame.size.width) * scaleX;
	windowFrame.size.width += widthDelta;
    }
    if (winArgs.heightSet) {
	float heightDelta = (winArgs.height - webViewFrame.size.height) * scaleY;
	windowFrame.size.height += heightDelta;
	windowFrame.origin.y -= heightDelta;
    }
    
    [bridge setWindowFrame:windowFrame];
    
    [bridge showWindow];
    
    KWQ_UNBLOCK_EXCEPTIONS;
}

void BrowserExtensionMac::setIconURL(const KURL &url)
{
    KWQ_BLOCK_EXCEPTIONS;
    [m_frame->bridge() setIconURL:url.getNSURL()];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void BrowserExtensionMac::setTypedIconURL(const KURL &url, const QString &type)
{
    KWQ_BLOCK_EXCEPTIONS;
    [m_frame->bridge() setIconURL:url.getNSURL() withType:type.getNSString()];
    KWQ_UNBLOCK_EXCEPTIONS;
}

int BrowserExtensionMac::getHistoryLength()
{
    return [m_frame->bridge() historyLength];
}

void BrowserExtensionMac::goBackOrForward(int distance)
{
    KWQ_BLOCK_EXCEPTIONS;
    [m_frame->bridge() goBackOrForward:distance];
    KWQ_UNBLOCK_EXCEPTIONS;
}

bool BrowserExtensionMac::canRunModal()
{
    KWQ_BLOCK_EXCEPTIONS;
    return [m_frame->bridge() canRunModal];
    KWQ_UNBLOCK_EXCEPTIONS;
    return false;
}

bool BrowserExtensionMac::canRunModalNow()
{
    KWQ_BLOCK_EXCEPTIONS;
    return [m_frame->bridge() canRunModalNow];
    KWQ_UNBLOCK_EXCEPTIONS;
    return false;
}

void BrowserExtensionMac::runModal()
{
    KWQ_BLOCK_EXCEPTIONS;
    [m_frame->bridge() runModal];
    KWQ_UNBLOCK_EXCEPTIONS;
}

}
