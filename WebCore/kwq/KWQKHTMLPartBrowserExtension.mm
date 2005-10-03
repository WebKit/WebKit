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

#include "config.h"
#import "KWQKHTMLPartBrowserExtension.h"

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

    NSString *frameName = urlArgs.frameName.length() == 0 ? nil : urlArgs.frameName.getNSString();
    
    WebCoreBridge *bridge;

    NSString *referrer;
    QString argsReferrer = urlArgs.metaData()["referrer"];
    if (argsReferrer.length() > 0) {
        referrer = argsReferrer.getNSString();
    } else {
        referrer = [_part->bridge() referrer];
    }

    ASSERT(!winArgs.dialog || urlArgs.frameName.isEmpty());

    if (partResult)
	*partResult = NULL;

    if (frameName != nil) {
	bridge = [_part->bridge() findFrameNamed:frameName];
	if (bridge != nil) {
	    if (!url.isEmpty()) {
		[bridge loadURL:url.getNSURL() referrer:referrer reload:urlArgs.reload userGesture:true target:nil triggeringEvent:nil form:nil formValues:nil];
	    }
	    [bridge focusWindow];
	    if (partResult) {
		*partResult = [bridge part];
	    }
	    return;
	}
    }
    
    if (winArgs.dialog)
        bridge = [_part->bridge() createModalDialogWithURL:url.getNSURL()];
    else
        bridge = [_part->bridge() createWindowWithURL:url.getNSURL() frameName:frameName];
    if (!bridge)
        return;
    
    if (!winArgs.toolBarsVisible) {
	[bridge setToolbarsVisible:NO];
    }
    
    if (!winArgs.statusBarVisible) {
	[bridge setStatusBarVisible:NO];
    }
    
    if (!winArgs.scrollbarsVisible) {
	[bridge setScrollbarsVisible:NO];
    }
    
    if (!winArgs.resizable) {
	[bridge setWindowIsResizable:NO];
    }
    
    if (winArgs.xSet || winArgs.ySet || winArgs.widthSet || winArgs.heightSet) {
	NSRect frame = [bridge windowFrame];
	NSRect contentRect = [bridge windowContentRect];
	
	if (winArgs.xSet) {
	    frame.origin.x = winArgs.x;
	}
	
	if (winArgs.ySet) {
	    float heightForFlip = NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]);
	    frame.origin.y = heightForFlip - (winArgs.y + frame.size.height);
	}
	
	if (winArgs.widthSet) {
	    frame.size.width += winArgs.width - contentRect.size.width;
	}
	
	if (winArgs.heightSet) {
	    float heightDelta = winArgs.height - contentRect.size.height;
	    frame.size.height += heightDelta;
	    frame.origin.y -= heightDelta;
	}
	
	[bridge setWindowFrame:frame];
    }
    
    [bridge showWindow];

    if ([bridge part]) {
	[bridge part]->setName(urlArgs.frameName);
    }
    
    if (partResult) {
	*partResult = [bridge part];
    }

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
