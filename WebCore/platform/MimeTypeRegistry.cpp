/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#include "MimeTypeRegistry.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include <ApplicationServices/ApplicationServices.h>
#include "WebCoreSystemInterface.h"

#include "StringHash.h"

namespace WebCore
{
static WTF::HashSet<String> *supportedImageResourceMIMETypes;
static WTF::HashSet<String> *supportedImageMIMETypes;
static WTF::HashSet<String> *supportedNonImageMIMETypes;
    
#if PLATFORM(CG)
extern String getMIMETypeForUTI(const String & uti);
#endif

static void initialiseSupportedImageMIMETypes()
{    
#if PLATFORM(CG)
    CFArrayRef supportedTypes = CGImageSourceCopyTypeIdentifiers();
    int cnt = CFArrayGetCount(supportedTypes);
    for(int i = 0; i < cnt; i++) {
        CFStringRef supportedType = (CFStringRef)CFArrayGetValueAtIndex(supportedTypes, i);
        String mimeType=getMIMETypeForUTI(supportedType);
        if (!mimeType.isEmpty()) {
            supportedImageMIMETypes->add(mimeType);
            supportedImageResourceMIMETypes->add(mimeType);
        }
        CFRelease(supportedType);
    }
    CFRelease(supportedTypes);
    
    //We only get one MIME type per UTI, hence our need to add these manually
    supportedImageMIMETypes->add("image/pjpeg");
    supportedImageResourceMIMETypes->add("image/pjpeg");
    
    //We don't want to try to treat all binary data as an image
    supportedImageMIMETypes->remove("application/octet-stream");
    supportedImageResourceMIMETypes->remove("application/octet-stream");
    
    //Don't treat pdf/postscript as images directly
    supportedImageMIMETypes->remove("application/pdf");
    supportedImageMIMETypes->remove("application/postscript");

#else
    //assume that all implementations at least support
    supportedImageMIMETypes->add("jpg", "image/jpeg");
    supportedImageMIMETypes->add("jpeg", "image/jpeg");
    supportedImageMIMETypes->add("png", "image/png");
    supportedImageMIMETypes->add("gif", "image/gif");
    supportedImageMIMETypes->add("bmp", "image/bmp");
#endif;
}

static void initialiseSupportedNonImageMimeTypes()
{
    supportedNonImageMIMETypes->add("text/html");
    supportedNonImageMIMETypes->add("text/xml");
    supportedNonImageMIMETypes->add("text/xsl");
    supportedNonImageMIMETypes->add("text/plain");
    supportedNonImageMIMETypes->add("text/");
    supportedNonImageMIMETypes->add("application/x-javascript");
    supportedNonImageMIMETypes->add("application/xml");
    supportedNonImageMIMETypes->add("application/xhtml+xml");
    supportedNonImageMIMETypes->add("application/rss+xml");
    supportedNonImageMIMETypes->add("application/atom+xml");
#if PLATFORM(MAC)
    supportedNonImageMIMETypes->add("application/x-webarchive");
#endif
    supportedNonImageMIMETypes->add("multipart/x-mixed-replace");
#ifdef SVG_SUPPORT
    supportedNonImageMIMETypes->add("image/svg+xml");
#endif
}

static void initialiseMimeTypeRegistry()
{
    supportedImageResourceMIMETypes = new WTF::HashSet<String>();
    supportedImageMIMETypes = new WTF::HashSet<String>();
    supportedNonImageMIMETypes = new WTF::HashSet<String>();
    
    initialiseSupportedNonImageMimeTypes();
    initialiseSupportedImageMIMETypes();
}

bool MimeTypeRegistry::isSupportedImageMIMEType(const String &mimeType)
{ 
    if (!supportedImageMIMETypes)
        initialiseMimeTypeRegistry();
    return !mimeType.isEmpty() && supportedImageMIMETypes->contains(mimeType); 
}

bool MimeTypeRegistry::isSupportedImageResourceMIMEType(const String &mimeType)
{ 
    if (!supportedImageResourceMIMETypes)
        initialiseMimeTypeRegistry();
    return !mimeType.isEmpty() && supportedImageResourceMIMETypes->contains(mimeType); 
}
    
bool MimeTypeRegistry::isSupportedNonImageMIMEType(const String &mimeType)
{
    if (!supportedNonImageMIMETypes)
        initialiseMimeTypeRegistry();
    return !mimeType.isEmpty() && supportedNonImageMIMETypes->contains(mimeType);
}

const HashSet<String> &MimeTypeRegistry::getSupportedImageMIMETypes()
{
    if (!supportedImageMIMETypes)
        initialiseMimeTypeRegistry();
    return *supportedImageMIMETypes;
}

const HashSet<String> &MimeTypeRegistry::getSupportedImageResourceMIMETypes()
{
    if (!supportedImageResourceMIMETypes)
        initialiseMimeTypeRegistry();
    return *supportedImageResourceMIMETypes;
}

const HashSet<String> &MimeTypeRegistry::getSupportedNonImageMIMETypes()
{
    if (!supportedNonImageMIMETypes)
        initialiseMimeTypeRegistry();
    return *supportedNonImageMIMETypes;
}

}
