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
#import "KWQKHTMLPartBrowserExtension.h"

#import <kxmlcore/Assertions.h>
#import "KWQExceptions.h"
#import "WebCoreBridge.h"
#import "khtml_part.h"

KHTMLPartBrowserExtension::KHTMLPartBrowserExtension(KHTMLPart *part)
    : _part(KWQ(part)), _browserInterface(_part)
{
}

void KHTMLPartBrowserExtension::openURLRequest(const KURL &url, 
					       const KParts::URLArgs &args)
{
    if (url.protocol().lower() == "javascript") {
	_part->createEmptyDocument();
	_part->replaceContentsWithScriptResult(url);
     } else {
	_part->openURLRequest(url, args);
    }
}

void KHTMLPartBrowserExtension::openURLNotify()
{
}

void KHTMLPartBrowserExtension::createNewWindow(const KURL &url, 
						const KParts::URLArgs &urlArgs) 
{
    createNewWindow(url, urlArgs, KParts::WindowArgs(), NULL);
}

void KHTMLPartBrowserExtension::createNewWindow(const KURL &url, 
						const KParts::URLArgs &urlArgs, 
						const KParts::WindowArgs &winArgs, 
						KParts::ReadOnlyPart *&part)
{
    createNewWindow(url, urlArgs, winArgs, &part);
}

void KHTMLPartBrowserExtension::createNewWindow(const KURL &url, 
						const KParts::URLArgs &urlArgs, 
						const KParts::WindowArgs &winArgs, 
						KParts::ReadOnlyPart **partResult)
{ 
    KWQ_BLOCK_EXCEPTIONS;

    ASSERT(!winArgs.dialog || urlArgs.frameName.isEmpty());

    if (partResult)
	*partResult = NULL;

    NSString *frameName = urlArgs.frameName.length() == 0 ? nil : urlArgs.frameName.getNSString();
    if (frameName) {
        // FIXME: Can't you just use _part->findFrame?
        if (WebCoreBridge *bridge = [_part->bridge() findFrameNamed:frameName]) {
            if (!url.isEmpty()) {
                QString argsReferrer = urlArgs.metaData()["referrer"];
                NSString *referrer;
                if (argsReferrer.length() > 0)
                    referrer = argsReferrer.getNSString();
                else
                    referrer = [_part->bridge() referrer];

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
                *partResult = [bridge part];

            return;
        }
    }
    
    WebCoreBridge *bridge;
    if (winArgs.dialog)
        bridge = [_part->bridge() createModalDialogWithURL:url.getNSURL()];
    else
        bridge = [_part->bridge() createWindowWithURL:url.getNSURL() frameName:frameName];

    if (!bridge)
        return;
    
    if ([bridge part])
	[bridge part]->setName(urlArgs.frameName);
    
    if (partResult)
	*partResult = [bridge part];
    
    [bridge setToolbarsVisible:winArgs.toolBarVisible || winArgs.locationBarVisible];
    [bridge setStatusbarVisible:winArgs.statusBarVisible];
    [bridge setScrollbarsVisible:winArgs.scrollBarsVisible];
    [bridge setWindowIsResizable:winArgs.resizable];
    
    NSRect windowFrame = [bridge windowFrame];

    NSSize scaleRect = [[bridge webView] convertSize:NSMakeSize(1, 1) toView:nil];
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

void KHTMLPartBrowserExtension::setIconURL(const KURL &url)
{
    KWQ_BLOCK_EXCEPTIONS;
    [_part->bridge() setIconURL:url.getNSURL()];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void KHTMLPartBrowserExtension::setTypedIconURL(const KURL &url, const QString &type)
{
    KWQ_BLOCK_EXCEPTIONS;
    [_part->bridge() setIconURL:url.getNSURL() withType:type.getNSString()];
    KWQ_UNBLOCK_EXCEPTIONS;
}

bool KHTMLPartBrowserExtension::canRunModal()
{
    KWQ_BLOCK_EXCEPTIONS;
    return [_part->bridge() canRunModal];
    KWQ_UNBLOCK_EXCEPTIONS;
    return false;
}

bool KHTMLPartBrowserExtension::canRunModalNow()
{
    KWQ_BLOCK_EXCEPTIONS;
    return [_part->bridge() canRunModalNow];
    KWQ_UNBLOCK_EXCEPTIONS;
    return false;
}

void KHTMLPartBrowserExtension::runModal()
{
    KWQ_BLOCK_EXCEPTIONS;
    [_part->bridge() runModal];
    KWQ_UNBLOCK_EXCEPTIONS;
}
