/*
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#import "FrameTree.h"
#import "BlockExceptions.h"
#import "FrameMac.h"
#import "WebCoreFrameBridge.h"
#import "WebCorePageBridge.h"
#import <kxmlcore/Assertions.h>

namespace WebCore {

BrowserExtensionMac::BrowserExtensionMac(Frame *frame)
    : m_frame(Mac(frame))
{
}

void BrowserExtensionMac::createNewWindow(const ResourceRequest& request) 
{
    createNewWindow(request, WindowArgs(), NULL);
}

void BrowserExtensionMac::createNewWindow(const ResourceRequest& request, 
                                          const WindowArgs& winArgs, 
                                          Frame*& part)
{
    createNewWindow(request, winArgs, &part);
}

void BrowserExtensionMac::createNewWindow(const ResourceRequest& request, 
                                          const WindowArgs& winArgs, 
                                          Frame** partResult)
{ 
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    ASSERT(!winArgs.dialog || request.frameName.isEmpty());

    if (partResult)
	*partResult = NULL;
    
    const KURL& url = request.url();

    NSString *frameName = request.frameName.length() == 0 ? nil : request.frameName.getNSString();
    if (frameName) {
        // FIXME: Can't we just use m_frame->findFrame?
        if (WebCoreFrameBridge *bridge = [m_frame->bridge() findFrameNamed:frameName]) {
            if (!url.isEmpty()) {
                String argsReferrer = request.referrer();
                NSString *referrer;
                if (!argsReferrer.isEmpty())
                    referrer = argsReferrer;
                else
                    referrer = [m_frame->bridge() referrer];

                [bridge loadURL:url.getNSURL() 
                       referrer:referrer 
                         reload:request.reload 
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
    
    WebCorePageBridge *page;
    if (winArgs.dialog)
        page = [m_frame->bridge() createModalDialogWithURL:url.getNSURL()];
    else
        page = [m_frame->bridge() createWindowWithURL:url.getNSURL()];
    if (!page)
        return;
    
    WebCoreFrameBridge *bridge = [page mainFrame];
    if ([bridge impl])
	[bridge impl]->tree()->setName(request.frameName);
    
    if (partResult)
	*partResult = [bridge impl];
    
    [bridge setToolbarsVisible:winArgs.toolBarVisible || winArgs.locationBarVisible];
    [bridge setStatusbarVisible:winArgs.statusBarVisible];
    [bridge setScrollbarsVisible:winArgs.scrollBarsVisible];
    [bridge setWindowIsResizable:winArgs.resizable];
    
    NSRect windowFrame = [page windowFrame];

    NSSize size = { 1, 1 }; // workaround for 4213314
    NSSize scaleRect = [[page outerView] convertSize:size toView:nil];
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
    NSRect webViewFrame = [[page outerView] frame];
    if (winArgs.widthSet) {
	float widthDelta = (winArgs.width - webViewFrame.size.width) * scaleX;
	windowFrame.size.width += widthDelta;
    }
    if (winArgs.heightSet) {
	float heightDelta = (winArgs.height - webViewFrame.size.height) * scaleY;
	windowFrame.size.height += heightDelta;
	windowFrame.origin.y -= heightDelta;
    }
    
    [page setWindowFrame:windowFrame];
    
    [bridge showWindow];
    
    END_BLOCK_OBJC_EXCEPTIONS;
}

void BrowserExtensionMac::setIconURL(const KURL &url)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [m_frame->bridge() setIconURL:url.getNSURL()];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void BrowserExtensionMac::setTypedIconURL(const KURL &url, const DeprecatedString &type)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [m_frame->bridge() setIconURL:url.getNSURL() withType:type.getNSString()];
    END_BLOCK_OBJC_EXCEPTIONS;
}

int BrowserExtensionMac::getHistoryLength()
{
    return [m_frame->bridge() historyLength];
}

void BrowserExtensionMac::goBackOrForward(int distance)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [m_frame->bridge() goBackOrForward:distance];
    END_BLOCK_OBJC_EXCEPTIONS;
}

bool BrowserExtensionMac::canRunModal()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [m_frame->bridge() canRunModal];
    END_BLOCK_OBJC_EXCEPTIONS;
    return false;
}

bool BrowserExtensionMac::canRunModalNow()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [m_frame->bridge() canRunModalNow];
    END_BLOCK_OBJC_EXCEPTIONS;
    return false;
}

void BrowserExtensionMac::runModal()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [m_frame->bridge() runModal];
    END_BLOCK_OBJC_EXCEPTIONS;
}

}
