/*
 * Copyright (C) 2001, 2002 Apple Computer, Inc.  All rights reserved.
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

#import <khtml_ext.h>
#import <khtml_part.h>
#import <WebCoreBridge.h>

KHTMLPartBrowserExtension::KHTMLPartBrowserExtension(KHTMLPart *part)
{
    m_part = part;
}

void KHTMLPartBrowserExtension::openURLRequest(const KURL &url, 
					       const KParts::URLArgs &args)
{
    m_part->impl->openURLRequest(url, args);
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
    NSString *frameName = urlArgs.frameName.length() == 0 ? nil : urlArgs.frameName.getNSString();

    WebCoreBridge *bridge;

    if (frameName != nil) {
	bridge = [m_part->impl->bridge() frameNamed:frameName];
	if (bridge != nil) {
	    if (!url.isEmpty()) {
		[bridge openURL:url.getNSURL()];
	    }
	    *partResult = [bridge part];
	    return;
	}
    }

    NSURL *cocoaURL = url.isEmpty() ? nil : url.getNSURL();
    bridge = [m_part->impl->bridge() openNewWindowWithURL:cocoaURL
         referrer:KWQKHTMLPartImpl::referrer(urlArgs)
         frameName:frameName];
    
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
	[[bridge window] setShowsResizeIndicator:NO];
    }
    
    if (winArgs.xSet || winArgs.ySet || winArgs.widthSet || winArgs.heightSet) {
	
	NSRect screenFrame = [[[bridge window] screen] frame];
	NSRect frame = [[bridge window] frame];
	
	if (winArgs.xSet) {
	    frame.origin.x = winArgs.x;
	}
	
	if (winArgs.ySet) {
	    if (winArgs.heightSet) {
		frame.origin.y = screenFrame.size.height - winArgs.y + frame.size.height - winArgs.height;
	    } else {
		frame.origin.y = screenFrame.size.height - winArgs.y;
	    }
	}
	
	if (winArgs.widthSet) {
	    frame.size.width = winArgs.width;
	}
	
	if (winArgs.heightSet) {
	    frame.size.height = winArgs.height;
	}
	
	[bridge setWindowFrame:frame];
    }
    
    *partResult = [bridge part];
}

void KHTMLPartBrowserExtension::setIconURL(const KURL &url)
{
    [m_part->impl->bridge() setIconURL:url.getNSURL()];
}

void KHTMLPartBrowserExtension::setTypedIconURL(const KURL &url, const QString &type)
{
    [m_part->impl->bridge() setIconURL:url.getNSURL() withType:type.getNSString()];
}
