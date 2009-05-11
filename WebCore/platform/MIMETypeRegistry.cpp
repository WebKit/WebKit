/*
 * Copyright (C) 2006, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#include "ArchiveFactory.h"
#include "MediaPlayer.h"
#include "StringHash.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/StdLibExtras.h>

#if PLATFORM(CG)
#include "ImageSourceCG.h"
#include <ApplicationServices/ApplicationServices.h>
#include <wtf/RetainPtr.h>
#endif
#if PLATFORM(QT)
#include <qimagereader.h>
#include <qimagewriter.h>
#endif

namespace WebCore {

static HashSet<String>* supportedImageResourceMIMETypes;
static HashSet<String>* supportedImageMIMETypes;
static HashSet<String>* supportedImageMIMETypesForEncoding;
static HashSet<String>* supportedJavaScriptMIMETypes;
static HashSet<String>* supportedNonImageMIMETypes;
static HashSet<String>* supportedMediaMIMETypes;

static void initializeSupportedImageMIMETypes()
{
#if PLATFORM(CG)
    RetainPtr<CFArrayRef> supportedTypes(AdoptCF, CGImageSourceCopyTypeIdentifiers());
    CFIndex count = CFArrayGetCount(supportedTypes.get());
    for (CFIndex i = 0; i < count; i++) {
        RetainPtr<CFStringRef> supportedType(AdoptCF, reinterpret_cast<CFStringRef>(CFArrayGetValueAtIndex(supportedTypes.get(), i)));
        String mimeType = MIMETypeForImageSourceType(supportedType.get());
        if (!mimeType.isEmpty()) {
            supportedImageMIMETypes->add(mimeType);
            supportedImageResourceMIMETypes->add(mimeType);
        }
    }

    // On Tiger and Leopard, com.microsoft.bmp doesn't have a MIME type in the registry.
    supportedImageMIMETypes->add("image/bmp");
    supportedImageResourceMIMETypes->add("image/bmp");

    // Favicons don't have a MIME type in the registry either.
    supportedImageMIMETypes->add("image/x-icon");
    supportedImageResourceMIMETypes->add("image/x-icon");

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

    supportedImageMIMETypes->remove("application/octet-stream");
    supportedImageResourceMIMETypes->remove("application/octet-stream");
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
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); ++i) {
        supportedImageMIMETypes->add(types[i]);
        supportedImageResourceMIMETypes->add(types[i]);
    }
#endif
}

static void initializeSupportedImageMIMETypesForEncoding()
{
    supportedImageMIMETypesForEncoding = new HashSet<String>;

#if PLATFORM(CG)
#if PLATFORM(MAC)
    RetainPtr<CFArrayRef> supportedTypes(AdoptCF, CGImageDestinationCopyTypeIdentifiers());
    CFIndex count = CFArrayGetCount(supportedTypes.get());
    for (CFIndex i = 0; i < count; i++) {
        RetainPtr<CFStringRef> supportedType(AdoptCF, reinterpret_cast<CFStringRef>(CFArrayGetValueAtIndex(supportedTypes.get(), i)));
        String mimeType = MIMETypeForImageSourceType(supportedType.get());
        if (!mimeType.isEmpty())
            supportedImageMIMETypesForEncoding->add(mimeType);
    }
#else
    // FIXME: Add Windows support for all the supported UTI's when a way to convert from MIMEType to UTI reliably is found.
    // For now, only support PNG, JPEG and GIF.  See <rdar://problem/6095286>.
    supportedImageMIMETypesForEncoding->add("image/png");
    supportedImageMIMETypesForEncoding->add("image/jpeg");
    supportedImageMIMETypesForEncoding->add("image/gif");
#endif
#elif PLATFORM(QT)
    QList<QByteArray> formats = QImageWriter::supportedImageFormats();
    for (size_t i = 0; i < formats.size(); ++i) {
        String mimeType = MIMETypeRegistry::getMIMETypeForExtension(formats.at(i).constData());
        supportedImageMIMETypesForEncoding->add(mimeType);
    }

    supportedImageMIMETypesForEncoding->remove("application/octet-stream");
#elif PLATFORM(CAIRO)
    supportedImageMIMETypesForEncoding->add("image/png");
#endif
}

static void initializeSupportedJavaScriptMIMETypes()
{
    /*
        Mozilla 1.8 and WinIE 7 both accept text/javascript and text/ecmascript.
        Mozilla 1.8 accepts application/javascript, application/ecmascript, and application/x-javascript, but WinIE 7 doesn't.
        WinIE 7 accepts text/javascript1.1 - text/javascript1.3, text/jscript, and text/livescript, but Mozilla 1.8 doesn't.
        Mozilla 1.8 allows leading and trailing whitespace, but WinIE 7 doesn't.
        Mozilla 1.8 and WinIE 7 both accept the empty string, but neither accept a whitespace-only string.
        We want to accept all the values that either of these browsers accept, but not other values.
     */
    static const char* types[] = {
        "text/javascript",
        "text/ecmascript",
        "application/javascript",
        "application/ecmascript",
        "application/x-javascript",
        "text/javascript1.1",
        "text/javascript1.2",
        "text/javascript1.3",
        "text/jscript",
        "text/livescript",
    };
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); ++i)
      supportedJavaScriptMIMETypes->add(types[i]);
}

static void initializeSupportedNonImageMimeTypes()
{
    static const char* types[] = {
#if ENABLE(WML)
        "text/vnd.wap.wml",
        "application/vnd.wap.wmlc",
#endif
        "text/html",
        "text/xml",
        "text/xsl",
        "text/plain",
        "text/",
        "application/xml",
        "application/xhtml+xml",
        "application/rss+xml",
        "application/atom+xml",
#if ENABLE(SVG)
        "image/svg+xml",
#endif
#if ENABLE(FTPDIR)
        "application/x-ftp-directory",
#endif
        "multipart/x-mixed-replace"
    };
    for (size_t i = 0; i < sizeof(types)/sizeof(types[0]); ++i)
        supportedNonImageMIMETypes->add(types[i]);

    ArchiveFactory::registerKnownArchiveMIMETypes();
}

static void initializeSupportedMediaMIMETypes()
{
    supportedMediaMIMETypes = new HashSet<String>;
#if ENABLE(VIDEO)
    MediaPlayer::getSupportedTypes(*supportedMediaMIMETypes);
#endif
}

static void initializeMIMETypeRegistry()
{
    supportedJavaScriptMIMETypes = new HashSet<String>;
    initializeSupportedJavaScriptMIMETypes();

    supportedNonImageMIMETypes = new HashSet<String>(*supportedJavaScriptMIMETypes);
    initializeSupportedNonImageMimeTypes();

    supportedImageResourceMIMETypes = new HashSet<String>;
    supportedImageMIMETypes = new HashSet<String>;
    initializeSupportedImageMIMETypes();
}

String MIMETypeRegistry::getMIMETypeForPath(const String& path)
{
    int pos = path.reverseFind('.');
    if (pos >= 0) {
        String extension = path.substring(pos + 1);
        String result = getMIMETypeForExtension(extension);
        if (result.length())
            return result;
    }
    return "application/octet-stream";
}

bool MIMETypeRegistry::isSupportedImageMIMEType(const String& mimeType)
{
    if (mimeType.isEmpty())
        return false;
    if (!supportedImageMIMETypes)
        initializeMIMETypeRegistry();
    return supportedImageMIMETypes->contains(mimeType);
}

bool MIMETypeRegistry::isSupportedImageResourceMIMEType(const String& mimeType)
{
    if (mimeType.isEmpty())
        return false;
    if (!supportedImageResourceMIMETypes)
        initializeMIMETypeRegistry();
    return supportedImageResourceMIMETypes->contains(mimeType);
}

bool MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(const String& mimeType)
{
    if (mimeType.isEmpty())
        return false;
    if (!supportedImageMIMETypesForEncoding)
        initializeSupportedImageMIMETypesForEncoding();
    return supportedImageMIMETypesForEncoding->contains(mimeType);
}

bool MIMETypeRegistry::isSupportedJavaScriptMIMEType(const String& mimeType)
{
    if (mimeType.isEmpty())
        return false;
    if (!supportedJavaScriptMIMETypes)
        initializeMIMETypeRegistry();
    return supportedJavaScriptMIMETypes->contains(mimeType);
}

bool MIMETypeRegistry::isSupportedNonImageMIMEType(const String& mimeType)
{
    if (mimeType.isEmpty())
        return false;
    if (!supportedNonImageMIMETypes)
        initializeMIMETypeRegistry();
    return supportedNonImageMIMETypes->contains(mimeType);
}

bool MIMETypeRegistry::isSupportedMediaMIMEType(const String& mimeType)
{
    if (mimeType.isEmpty())
        return false;
    if (!supportedMediaMIMETypes)
        initializeSupportedMediaMIMETypes();
    return supportedMediaMIMETypes->contains(mimeType);
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

HashSet<String>& MIMETypeRegistry::getSupportedImageMIMETypes()
{
    if (!supportedImageMIMETypes)
        initializeMIMETypeRegistry();
    return *supportedImageMIMETypes;
}

HashSet<String>& MIMETypeRegistry::getSupportedImageResourceMIMETypes()
{
    if (!supportedImageResourceMIMETypes)
        initializeMIMETypeRegistry();
    return *supportedImageResourceMIMETypes;
}

HashSet<String>& MIMETypeRegistry::getSupportedImageMIMETypesForEncoding()
{
    if (!supportedImageMIMETypesForEncoding)
        initializeSupportedImageMIMETypesForEncoding();
    return *supportedImageMIMETypesForEncoding;
}

HashSet<String>& MIMETypeRegistry::getSupportedNonImageMIMETypes()
{
    if (!supportedNonImageMIMETypes)
        initializeMIMETypeRegistry();
    return *supportedNonImageMIMETypes;
}

HashSet<String>& MIMETypeRegistry::getSupportedMediaMIMETypes()
{
    if (!supportedMediaMIMETypes)
        initializeSupportedMediaMIMETypes();
    return *supportedMediaMIMETypes;
}

const String& defaultMIMEType()
{
    DEFINE_STATIC_LOCAL(const String, defaultMIMEType, ("application/octet-stream"));
    return defaultMIMEType;
}

} // namespace WebCore
