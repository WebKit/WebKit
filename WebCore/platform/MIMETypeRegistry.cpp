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
#include "MIMETypeRegistry.h"

#include "StringHash.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#if PLATFORM(CG)
#include <ApplicationServices/ApplicationServices.h>
#endif
#if PLATFORM(MAC)
#include "WebCoreSystemInterface.h"
#endif
#if PLATFORM(QT)
#include <qimagereader.h>
#endif

namespace WebCore
{
static WTF::HashSet<String>* supportedImageResourceMIMETypes;
static WTF::HashSet<String>* supportedImageMIMETypes;
static WTF::HashSet<String>* supportedNonImageMIMETypes;
    
#if PLATFORM(CG)
extern String getMIMETypeForUTI(const String& uti);
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
    
    // On Tiger, com.microsoft.bmp doesn't have a MIME type in the registry.
    supportedImageMIMETypes->add("image/bmp");
    supportedImageResourceMIMETypes->add("image/bmp");
    
    //  We only get one MIME type per UTI, hence our need to add these manually
    supportedImageMIMETypes->add("image/pjpeg");
    supportedImageResourceMIMETypes->add("image/pjpeg");
    
    //  We don't want to try to treat all binary data as an image
    supportedImageMIMETypes->remove("application/octet-stream");
    supportedImageResourceMIMETypes->remove("application/octet-stream");
    
    //  Don't treat pdf/postscript as images directly
    supportedImageMIMETypes->remove("application/pdf");
    supportedImageMIMETypes->remove("application/postscript");

#elif PLATFORM(QT)
    QList<QByteArray> formats = QImageReader::supportedImageFormats();
    for (size_t i = 0; i < formats.size(); ++i) {
#if ENABLE(SVG) 
        /*
         * Qt has support for SVG, but we want to use KSVG2
         */
        if (formats.at(i).toLower().startsWith("svg"))
            continue;
#endif
        String mimeType = MIMETypeRegistry::getMIMETypeForExtension(formats.at(i).constData());
        supportedImageMIMETypes->add(mimeType);
        supportedImageResourceMIMETypes->add(mimeType);
    }
#else
    // assume that all implementations at least support the following standard
    // image types:
    static const char* types[] = {
      "image/jpeg",
      "image/png",
      "image/gif",
      "image/bmp",
      "image/x-icon",    // ico
      "image/x-xbitmap"  // xbm
    };
    for (size_t i = 0; i < sizeof(types)/sizeof(types[0]); ++i) {
      supportedImageMIMETypes->add(types[i]);
      supportedImageResourceMIMETypes->add(types[i]);
    }
#endif
}

static void initialiseSupportedNonImageMimeTypes()
{
    static const char* types[] = {
      "text/html",
      "text/xml",
      "text/xsl",
      "text/plain",
      "text/",
      "application/x-javascript",
      "application/xml",
      "application/xhtml+xml",
      "application/rss+xml",
      "application/atom+xml",
#if PLATFORM(MAC)
      "application/x-webarchive",
#endif
      "multipart/x-mixed-replace"
#if ENABLE(SVG)
      , "image/svg+xml"
#endif
#if ENABLE(FTPDIR)
      , "application/x-ftp-directory"
#endif
    };
    for (size_t i = 0; i < sizeof(types)/sizeof(types[0]); ++i)
      supportedNonImageMIMETypes->add(types[i]);
}

static void initialiseMIMETypeRegistry()
{
    supportedImageResourceMIMETypes = new WTF::HashSet<String>();
    supportedImageMIMETypes = new WTF::HashSet<String>();
    supportedNonImageMIMETypes = new WTF::HashSet<String>();
    
    initialiseSupportedNonImageMimeTypes();
    initialiseSupportedImageMIMETypes();
}

String MIMETypeRegistry::getMIMETypeForPath(const String& path)
{
    int pos = path.reverseFind('.');
    if(pos >= 0) {
        String extension = path.substring(pos + 1);
        return getMIMETypeForExtension(extension);
    }
    return "application/octet-stream";
}

bool MIMETypeRegistry::isSupportedImageMIMEType(const String& mimeType)
{ 
    if (!supportedImageMIMETypes)
        initialiseMIMETypeRegistry();
    return !mimeType.isEmpty() && supportedImageMIMETypes->contains(mimeType); 
}

bool MIMETypeRegistry::isSupportedImageResourceMIMEType(const String& mimeType)
{ 
    if (!supportedImageResourceMIMETypes)
        initialiseMIMETypeRegistry();
    return !mimeType.isEmpty() && supportedImageResourceMIMETypes->contains(mimeType); 
}
    
bool MIMETypeRegistry::isSupportedNonImageMIMEType(const String& mimeType)
{
    if (!supportedNonImageMIMETypes)
        initialiseMIMETypeRegistry();
    return !mimeType.isEmpty() && supportedNonImageMIMETypes->contains(mimeType);
}

bool MIMETypeRegistry::isJavaAppletMIMEType(const String& mimeType)
{
    // Since this set is very limited and is likely to remain so we won't bother with the overhead
    // of using a hash set.
    // Any of the MIME types below may be followed by any number of specific versions of the JVM,
    // which is why we use startsWith()
    return mimeType.startsWith("application/x-java-applet", false) 
        || mimeType.startsWith("application/x-java-bean", false) 
        || mimeType.startsWith("application/x-java-vm", false);
}

HashSet<String> &MIMETypeRegistry::getSupportedImageMIMETypes()
{
    if (!supportedImageMIMETypes)
        initialiseMIMETypeRegistry();
    return *supportedImageMIMETypes;
}

HashSet<String> &MIMETypeRegistry::getSupportedImageResourceMIMETypes()
{
    if (!supportedImageResourceMIMETypes)
        initialiseMIMETypeRegistry();
    return *supportedImageResourceMIMETypes;
}

HashSet<String> &MIMETypeRegistry::getSupportedNonImageMIMETypes()
{
    if (!supportedNonImageMIMETypes)
        initialiseMIMETypeRegistry();
    return *supportedNonImageMIMETypes;
}

}
