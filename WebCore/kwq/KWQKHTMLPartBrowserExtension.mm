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

#import <khtml/khtml_ext.h>
#import <khtml_part.h>
#import <WebCoreBridge.h>
#import <KWQKHTMLPartImpl.h>

KHTMLPartBrowserExtension::KHTMLPartBrowserExtension(KHTMLPart *part)
{
    m_part = part;
}

void KHTMLPartBrowserExtension::openURLRequest(const KURL &url, 
					       const KParts::URLArgs &args)
{
    m_part->openURLInFrame(url, args);
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
    [m_part->impl->getBridge() openNewWindowWithURL:url.getNSURL()];
    
    // We can't return a KHTMLPart in all cases, because the new window might not even
    // have HTML in it. And we don't create the KHTMLPart until we become "committed".
    // So it's better not to try to return the KHTMLPart, and no callers currently need it.
    if (partResult) {
	*partResult = 0;
    }
}
