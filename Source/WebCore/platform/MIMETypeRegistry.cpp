/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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

#include "MediaPlayer.h"
#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>

#if USE(CG)
#include "ImageSourceCG.h"
#include "UTIRegistry.h"
#include <ImageIO/ImageIO.h>
#include <wtf/RetainPtr.h>
#endif

#if USE(CG) && PLATFORM(COCOA)
#include "UTIUtilities.h"
#endif

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
#include "ArchiveFactory.h"
#endif

#if HAVE(AVASSETREADER)
#include "ContentType.h"
#include "ImageDecoderAVFObjC.h"
#endif

namespace WebCore {

static HashSet<String, ASCIICaseInsensitiveHash>* supportedImageResourceMIMETypes;
static HashSet<String, ASCIICaseInsensitiveHash>* supportedImageMIMETypes;
static HashSet<String, ASCIICaseInsensitiveHash>* supportedImageMIMETypesForEncoding;
static HashSet<String, ASCIICaseInsensitiveHash>* supportedJavaScriptMIMETypes;
static HashSet<String, ASCIICaseInsensitiveHash>* supportedNonImageMIMETypes;
static HashSet<String, ASCIICaseInsensitiveHash>* supportedMediaMIMETypes;
static HashSet<String, ASCIICaseInsensitiveHash>* pdfMIMETypes;
static HashSet<String, ASCIICaseInsensitiveHash>* unsupportedTextMIMETypes;

static void initializeSupportedImageMIMETypes()
{
    supportedImageResourceMIMETypes = new HashSet<String, ASCIICaseInsensitiveHash>;
    supportedImageMIMETypes = new HashSet<String, ASCIICaseInsensitiveHash>;

#if USE(CG)
    // This represents the subset of allowed image UTIs for which CoreServices has a corresponding MIME type. Keep this in sync with allowedImageUTIs().
    static const char* const allowedImageMIMETypes[] = { "image/tiff", "image/gif", "image/jpeg", "image/vnd.microsoft.icon", "image/jp2", "image/png", "image/bmp" };
    for (auto& mimeType : allowedImageMIMETypes) {
        supportedImageMIMETypes->add(ASCIILiteral { mimeType });
        supportedImageResourceMIMETypes->add(ASCIILiteral { mimeType });
    }

#ifndef NDEBUG
    for (auto& uti : allowedImageUTIs()) {
        auto mimeType = MIMETypeForImageSourceType(uti);
        if (!mimeType.isEmpty()) {
            ASSERT(supportedImageMIMETypes->contains(mimeType));
            ASSERT(supportedImageResourceMIMETypes->contains(mimeType));
        }
    }

#if PLATFORM(COCOA)
    for (auto& mime : *supportedImageMIMETypes)
        ASSERT_UNUSED(mime, allowedImageUTIs().contains(UTIFromMIMEType(mime)));
#endif
#endif

    // Favicons don't have a MIME type in the registry either.
    supportedImageMIMETypes->add("image/x-icon");
    supportedImageResourceMIMETypes->add("image/x-icon");

    //  We only get one MIME type per UTI, hence our need to add these manually
    supportedImageMIMETypes->add("image/pjpeg");
    supportedImageResourceMIMETypes->add("image/pjpeg");

#if PLATFORM(IOS)
    // Add malformed image mimetype for compatibility with Mail and to handle malformed mimetypes from the net
    // These were removed for <rdar://problem/6564538> Re-enable UTI code in WebCore now that MobileCoreServices exists
    // But Mail relies on at least image/tif reported as being supported (should be image/tiff).
    // This can be removed when Mail addresses:
    // <rdar://problem/7879510> Mail should use standard image mimetypes
    // and we fix sniffing so that it corrects items such as image/jpg -> image/jpeg.
    static const char* const malformedMIMETypes[] = {
        // JPEG (image/jpeg)
        "image/jpg", "image/jp_", "image/jpe_", "application/jpg", "application/x-jpg", "image/pipeg",
        "image/vnd.switfview-jpeg", "image/x-xbitmap",
        // GIF (image/gif)
        "image/gi_",
        // PNG (image/png)
        "application/png", "application/x-png",
        // TIFF (image/tiff)
        "image/x-tif", "image/tif", "image/x-tiff", "application/tif", "application/x-tif", "application/tiff",
        "application/x-tiff",
        // BMP (image/bmp, image/x-bitmap)
        "image/x-bmp", "image/x-win-bitmap", "image/x-windows-bmp", "image/ms-bmp", "image/x-ms-bmp",
        "application/bmp", "application/x-bmp", "application/x-win-bitmap",
    };
    for (auto& type : malformedMIMETypes) {
        supportedImageMIMETypes->add(type);
        supportedImageResourceMIMETypes->add(type);
    }
#endif

#else
    // assume that all implementations at least support the following standard
    // image types:
    static const char* const types[] = {
        "image/jpeg",
        "image/png",
        "image/gif",
        "image/bmp",
        "image/vnd.microsoft.icon",    // ico
        "image/x-icon",    // ico
        "image/x-xbitmap"  // xbm
    };
    for (auto& type : types) {
        supportedImageMIMETypes->add(type);
        supportedImageResourceMIMETypes->add(type);
    }

#if USE(WEBP)
    supportedImageMIMETypes->add("image/webp");
    supportedImageResourceMIMETypes->add("image/webp");
#endif

#endif // USE(CG)
}

static void initializeSupportedImageMIMETypesForEncoding()
{
    supportedImageMIMETypesForEncoding = new HashSet<String, ASCIICaseInsensitiveHash>;

#if USE(CG)
#if PLATFORM(COCOA)
    RetainPtr<CFArrayRef> supportedTypes = adoptCF(CGImageDestinationCopyTypeIdentifiers());
    CFIndex count = CFArrayGetCount(supportedTypes.get());
    for (CFIndex i = 0; i < count; i++) {
        CFStringRef supportedType = reinterpret_cast<CFStringRef>(CFArrayGetValueAtIndex(supportedTypes.get(), i));
        String mimeType = MIMETypeForImageSourceType(supportedType);
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
#elif PLATFORM(GTK)
    supportedImageMIMETypesForEncoding->add("image/png");
    supportedImageMIMETypesForEncoding->add("image/jpeg");
    supportedImageMIMETypesForEncoding->add("image/tiff");
    supportedImageMIMETypesForEncoding->add("image/bmp");
    supportedImageMIMETypesForEncoding->add("image/ico");
#elif USE(CAIRO)
    supportedImageMIMETypesForEncoding->add("image/png");
#endif
}

static void initializeSupportedJavaScriptMIMETypes()
{
    // https://html.spec.whatwg.org/multipage/scripting.html#javascript-mime-type
    static const char* const types[] = {
        "text/javascript",
        "text/ecmascript",
        "application/javascript",
        "application/ecmascript",
        "application/x-javascript",
        "application/x-ecmascript",
        "text/javascript1.0",
        "text/javascript1.1",
        "text/javascript1.2",
        "text/javascript1.3",
        "text/javascript1.4",
        "text/javascript1.5",
        "text/jscript",
        "text/livescript",
        "text/x-javascript",
        "text/x-ecmascript"
    };

    supportedJavaScriptMIMETypes = new HashSet<String, ASCIICaseInsensitiveHash>;
    for (auto* type : types)
        supportedJavaScriptMIMETypes->add(type);
}

static void initializePDFMIMETypes()
{
    const char* const types[] = {
        "application/pdf",
        "text/pdf"
    };

    pdfMIMETypes = new HashSet<String, ASCIICaseInsensitiveHash>;
    for (auto& type : types)
        pdfMIMETypes->add(type);
}

static void initializeSupportedNonImageMimeTypes()
{
    static const char* const types[] = {
        "text/html",
        "text/xml",
        "text/xsl",
        "text/plain",
        "text/",
        "application/xml",
        "application/xhtml+xml",
#if !PLATFORM(IOS)
        "application/vnd.wap.xhtml+xml",
        "application/rss+xml",
        "application/atom+xml",
#endif
        "application/json",
        "image/svg+xml",
#if ENABLE(FTPDIR)
        "application/x-ftp-directory",
#endif
        "multipart/x-mixed-replace"
        // Note: Adding a new type here will probably render it as HTML.
        // This can result in cross-site scripting vulnerabilities.
    };

    if (!supportedJavaScriptMIMETypes)
        initializeSupportedJavaScriptMIMETypes();

    supportedNonImageMIMETypes = new HashSet<String, ASCIICaseInsensitiveHash> { *supportedJavaScriptMIMETypes };
    for (auto& type : types)
        supportedNonImageMIMETypes->add(type);

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
    ArchiveFactory::registerKnownArchiveMIMETypes();
#endif
}

static const Vector<String>* typesForCommonExtension(const String& extension)
{
    static const auto map = makeNeverDestroyed([] {
        struct TypeExtensionPair {
            const char* type;
            const char* extension;
        };

        // A table of common media MIME types and file extentions used when a platform's
        // specific MIME type lookup doesn't have a match for a media file extension.
        static const TypeExtensionPair commonMediaTypes[] = {
            // Ogg
            { "application/ogg", "ogx" },
            { "audio/ogg", "ogg" },
            { "audio/ogg", "oga" },
            { "video/ogg", "ogv" },

            // Annodex
            { "application/annodex", "anx" },
            { "audio/annodex", "axa" },
            { "video/annodex", "axv" },
            { "audio/speex", "spx" },

            // WebM
            { "video/webm", "webm" },
            { "audio/webm", "webm" },

            // MPEG
            { "audio/mpeg", "m1a" },
            { "audio/mpeg", "m2a" },
            { "audio/mpeg", "m1s" },
            { "audio/mpeg", "mpa" },
            { "video/mpeg", "mpg" },
            { "video/mpeg", "m15" },
            { "video/mpeg", "m1s" },
            { "video/mpeg", "m1v" },
            { "video/mpeg", "m75" },
            { "video/mpeg", "mpa" },
            { "video/mpeg", "mpeg" },
            { "video/mpeg", "mpm" },
            { "video/mpeg", "mpv" },

            // MPEG playlist
            { "application/vnd.apple.mpegurl", "m3u8" },
            { "application/mpegurl", "m3u8" },
            { "application/x-mpegurl", "m3u8" },
            { "audio/mpegurl", "m3url" },
            { "audio/x-mpegurl", "m3url" },
            { "audio/mpegurl", "m3u" },
            { "audio/x-mpegurl", "m3u" },

            // MPEG-4
            { "video/x-m4v", "m4v" },
            { "audio/x-m4a", "m4a" },
            { "audio/x-m4b", "m4b" },
            { "audio/x-m4p", "m4p" },
            { "audio/mp4", "m4a" },

            // MP3
            { "audio/mp3", "mp3" },
            { "audio/x-mp3", "mp3" },
            { "audio/x-mpeg", "mp3" },

            // MPEG-2
            { "video/x-mpeg2", "mp2" },
            { "video/mpeg2", "vob" },
            { "video/mpeg2", "mod" },
            { "video/m2ts", "m2ts" },
            { "video/x-m2ts", "m2t" },
            { "video/x-m2ts", "ts" },

            // 3GP/3GP2
            { "audio/3gpp", "3gpp" },
            { "audio/3gpp2", "3g2" },
            { "application/x-mpeg", "amc" },

            // AAC
            { "audio/aac", "aac" },
            { "audio/aac", "adts" },
            { "audio/x-aac", "m4r" },

            // CoreAudio File
            { "audio/x-caf", "caf" },
            { "audio/x-gsm", "gsm" },

            // ADPCM
            { "audio/x-wav", "wav" },
            { "audio/vnd.wave", "wav" },
        };

        HashMap<String, Vector<String>, ASCIICaseInsensitiveHash> map;
        for (auto& pair : commonMediaTypes) {
            const char* type = pair.type;
            const char* extension = pair.extension;
            map.ensure(ASCIILiteral { extension }, [type, extension] {
                // First type in the vector must always be the one from getMIMETypeForExtension,
                // so we can use the map without also calling getMIMETypeForExtension each time.
                Vector<String> synonyms;
                String systemType = MIMETypeRegistry::getMIMETypeForExtension(extension);
                if (!systemType.isEmpty() && type != systemType)
                    synonyms.append(systemType);
                return synonyms;
            }).iterator->value.append(ASCIILiteral { type });
        }
        return map;
    }());
    auto mapEntry = map.get().find(extension);
    if (mapEntry == map.get().end())
        return nullptr;
    return &mapEntry->value;
}

String MIMETypeRegistry::getMediaMIMETypeForExtension(const String& extension)
{
    auto* vector = typesForCommonExtension(extension);
    if (vector)
        return (*vector)[0];
    return getMIMETypeForExtension(extension);
}

Vector<String> MIMETypeRegistry::getMediaMIMETypesForExtension(const String& extension)
{
    auto* vector = typesForCommonExtension(extension);
    if (vector)
        return *vector;
    String type = getMIMETypeForExtension(extension);
    if (!type.isNull())
        return { { type } };
    return { };
}

static void initializeSupportedMediaMIMETypes()
{
    supportedMediaMIMETypes = new HashSet<String, ASCIICaseInsensitiveHash>;
#if ENABLE(VIDEO)
    MediaPlayer::getSupportedTypes(*supportedMediaMIMETypes);
#endif
}

static void initializeUnsupportedTextMIMETypes()
{
    static const char* const types[] = {
        "text/calendar",
        "text/x-calendar",
        "text/x-vcalendar",
        "text/vcalendar",
        "text/vcard",
        "text/x-vcard",
        "text/directory",
        "text/ldif",
        "text/qif",
        "text/x-qif",
        "text/x-csv",
        "text/x-vcf",
#if !PLATFORM(IOS)
        "text/rtf",
#else
        "text/vnd.sun.j2me.app-descriptor",
#endif
    };

    unsupportedTextMIMETypes = new HashSet<String, ASCIICaseInsensitiveHash>;
    for (auto& type : types)
        unsupportedTextMIMETypes->add(ASCIILiteral { type });
}

String MIMETypeRegistry::getMIMETypeForPath(const String& path)
{
    size_t pos = path.reverseFind('.');
    if (pos != notFound) {
        String extension = path.substring(pos + 1);
        String result = getMIMETypeForExtension(extension);
        if (result.length())
            return result;
    }
    return defaultMIMEType();
}

bool MIMETypeRegistry::isSupportedImageMIMEType(const String& mimeType)
{
    if (mimeType.isEmpty())
        return false;
    if (!supportedImageMIMETypes)
        initializeSupportedImageMIMETypes();
    return supportedImageMIMETypes->contains(getNormalizedMIMEType(mimeType));
}

bool MIMETypeRegistry::isSupportedImageVideoOrSVGMIMEType(const String& mimeType)
{
    if (isSupportedImageMIMEType(mimeType) || equalLettersIgnoringASCIICase(mimeType, "image/svg+xml"))
        return true;

#if HAVE(AVASSETREADER)
    if (ImageDecoderAVFObjC::supportsContentType(ContentType(mimeType)))
        return true;
#endif

    return false;
}

bool MIMETypeRegistry::isSupportedImageResourceMIMEType(const String& mimeType)
{
    if (mimeType.isEmpty())
        return false;
    if (!supportedImageResourceMIMETypes)
        initializeSupportedImageMIMETypes();
    return supportedImageResourceMIMETypes->contains(getNormalizedMIMEType(mimeType));
}

bool MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(const String& mimeType)
{
    ASSERT(isMainThread());

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

    if (!isMainThread()) {
        bool isSupported = false;
        callOnMainThreadAndWait([&isSupported, mimeType = mimeType.isolatedCopy()] {
            isSupported = isSupportedJavaScriptMIMEType(mimeType);
        });
        return isSupported;
    }

    if (!supportedJavaScriptMIMETypes)
        initializeSupportedNonImageMimeTypes();
    return supportedJavaScriptMIMETypes->contains(mimeType);
}

bool MIMETypeRegistry::isSupportedStyleSheetMIMEType(const String& mimeType)
{
    return equalLettersIgnoringASCIICase(mimeType, "text/css");
}

bool MIMETypeRegistry::isSupportedFontMIMEType(const String& mimeType)
{
    static const unsigned fontLength = 5;
    if (!startsWithLettersIgnoringASCIICase(mimeType, "font/"))
        return false;
    auto subtype = StringView { mimeType }.substring(fontLength);
    return equalLettersIgnoringASCIICase(subtype, "woff")
        || equalLettersIgnoringASCIICase(subtype, "woff2")
        || equalLettersIgnoringASCIICase(subtype, "otf")
        || equalLettersIgnoringASCIICase(subtype, "ttf")
        || equalLettersIgnoringASCIICase(subtype, "sfnt");
}

bool MIMETypeRegistry::isSupportedJSONMIMEType(const String& mimeType)
{
    if (mimeType.isEmpty())
        return false;

    if (equalLettersIgnoringASCIICase(mimeType, "application/json"))
        return true;

    // When detecting +json ensure there is a non-empty type / subtype preceeding the suffix.
    if (mimeType.endsWithIgnoringASCIICase("+json") && mimeType.length() >= 8) {
        size_t slashPosition = mimeType.find('/');
        if (slashPosition != notFound && slashPosition > 0 && slashPosition <= mimeType.length() - 6)
            return true;
    }

    return false;
}

bool MIMETypeRegistry::isSupportedNonImageMIMEType(const String& mimeType)
{
    if (mimeType.isEmpty())
        return false;
    if (!supportedNonImageMIMETypes)
        initializeSupportedNonImageMimeTypes();
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

bool MIMETypeRegistry::isSupportedTextTrackMIMEType(const String& mimeType)
{
    return equalLettersIgnoringASCIICase(mimeType, "text/vtt");
}

bool MIMETypeRegistry::isUnsupportedTextMIMEType(const String& mimeType)
{
    if (mimeType.isEmpty())
        return false;
    if (!unsupportedTextMIMETypes)
        initializeUnsupportedTextMIMETypes();
    return unsupportedTextMIMETypes->contains(mimeType);
}

bool MIMETypeRegistry::isTextMIMEType(const String& mimeType)
{
    return isSupportedJavaScriptMIMEType(mimeType)
        || isSupportedJSONMIMEType(mimeType) // Render JSON as text/plain.
        || (startsWithLettersIgnoringASCIICase(mimeType, "text/")
            && !equalLettersIgnoringASCIICase(mimeType, "text/html")
            && !equalLettersIgnoringASCIICase(mimeType, "text/xml")
            && !equalLettersIgnoringASCIICase(mimeType, "text/xsl"));
}

static inline bool isValidXMLMIMETypeChar(UChar c)
{
    // Valid characters per RFCs 3023 and 2045: 0-9a-zA-Z_-+~!$^{}|.%'`#&*
    return isASCIIAlphanumeric(c) || c == '!' || c == '#' || c == '$' || c == '%' || c == '&' || c == '\'' || c == '*' || c == '+'
        || c == '-' || c == '.' || c == '^' || c == '_' || c == '`' || c == '{' || c == '|' || c == '}' || c == '~';
}

bool MIMETypeRegistry::isXMLMIMEType(const String& mimeType)
{
    if (equalLettersIgnoringASCIICase(mimeType, "text/xml") || equalLettersIgnoringASCIICase(mimeType, "application/xml") || equalLettersIgnoringASCIICase(mimeType, "text/xsl"))
        return true;

    if (!mimeType.endsWithIgnoringASCIICase("+xml"))
        return false;

    size_t slashPosition = mimeType.find('/');
    // Take into account the '+xml' ending of mimeType.
    if (slashPosition == notFound || !slashPosition || slashPosition == mimeType.length() - 5)
        return false;

    // Again, mimeType ends with '+xml', no need to check the validity of that substring.
    size_t mimeLength = mimeType.length();
    for (size_t i = 0; i < mimeLength - 4; ++i) {
        if (!isValidXMLMIMETypeChar(mimeType[i]) && i != slashPosition)
            return false;
    }

    return true;
}

bool MIMETypeRegistry::isJavaAppletMIMEType(const String& mimeType)
{
    // Since this set is very limited and is likely to remain so we won't bother with the overhead
    // of using a hash set.
    // Any of the MIME types below may be followed by any number of specific versions of the JVM,
    // which is why we use startsWith()
    return startsWithLettersIgnoringASCIICase(mimeType, "application/x-java-applet")
        || startsWithLettersIgnoringASCIICase(mimeType, "application/x-java-bean")
        || startsWithLettersIgnoringASCIICase(mimeType, "application/x-java-vm");
}

bool MIMETypeRegistry::isPDFMIMEType(const String& mimeType)
{
    if (mimeType.isEmpty())
        return false;
    if (!pdfMIMETypes)
        initializePDFMIMETypes();
    return pdfMIMETypes->contains(mimeType);
}

bool MIMETypeRegistry::isPostScriptMIMEType(const String& mimeType)
{
    return mimeType == "application/postscript";
}

bool MIMETypeRegistry::isPDFOrPostScriptMIMEType(const String& mimeType)
{
    return isPDFMIMEType(mimeType) || isPostScriptMIMEType(mimeType);
}

bool MIMETypeRegistry::canShowMIMEType(const String& mimeType)
{
    if (isSupportedImageMIMEType(mimeType) || isSupportedNonImageMIMEType(mimeType) || isSupportedMediaMIMEType(mimeType))
        return true;

    if (isSupportedJavaScriptMIMEType(mimeType) || isSupportedJSONMIMEType(mimeType))
        return true;

    if (startsWithLettersIgnoringASCIICase(mimeType, "text/"))
        return !isUnsupportedTextMIMEType(mimeType);

    return false;
}

const HashSet<String, ASCIICaseInsensitiveHash>& MIMETypeRegistry::getSupportedImageMIMETypes()
{
    if (!supportedImageMIMETypes)
        initializeSupportedImageMIMETypes();
    return *supportedImageMIMETypes;
}

const HashSet<String, ASCIICaseInsensitiveHash>& MIMETypeRegistry::getSupportedImageResourceMIMETypes()
{
    if (!supportedImageResourceMIMETypes)
        initializeSupportedImageMIMETypes();
    return *supportedImageResourceMIMETypes;
}

HashSet<String, ASCIICaseInsensitiveHash>& MIMETypeRegistry::getSupportedNonImageMIMETypes()
{
    if (!supportedNonImageMIMETypes)
        initializeSupportedNonImageMimeTypes();
    return *supportedNonImageMIMETypes;
}

const HashSet<String, ASCIICaseInsensitiveHash>& MIMETypeRegistry::getSupportedMediaMIMETypes()
{
    if (!supportedMediaMIMETypes)
        initializeSupportedMediaMIMETypes();
    return *supportedMediaMIMETypes;
}


const HashSet<String, ASCIICaseInsensitiveHash>& MIMETypeRegistry::getPDFMIMETypes()
{
    if (!pdfMIMETypes)
        initializePDFMIMETypes();
    return *pdfMIMETypes;
}

const HashSet<String, ASCIICaseInsensitiveHash>& MIMETypeRegistry::getUnsupportedTextMIMETypes()
{
    if (!unsupportedTextMIMETypes)
        initializeUnsupportedTextMIMETypes();
    return *unsupportedTextMIMETypes;
}

const String& defaultMIMEType()
{
    static NeverDestroyed<const String> defaultMIMEType(MAKE_STATIC_STRING_IMPL("application/octet-stream"));
    return defaultMIMEType;
}

#if !USE(CURL)

// FIXME: Not sure why it makes sense to have a cross-platform function when only CURL has the concept
// of a "normalized" MIME type.
String MIMETypeRegistry::getNormalizedMIMEType(const String& mimeType)
{
    return mimeType;
}

#else

String MIMETypeRegistry::getNormalizedMIMEType(const String& mimeType)
{
    static const auto mimeTypeAssociationMap = makeNeverDestroyed([] {
        static const std::pair<const char*, const char*> mimeTypeAssociations[] = {
            { "image/x-ms-bmp", "image/bmp" },
            { "image/x-windows-bmp", "image/bmp" },
            { "image/x-bmp", "image/bmp" },
            { "image/x-bitmap", "image/bmp" },
            { "image/x-ms-bitmap", "image/bmp" },
            { "image/jpg", "image/jpeg" },
            { "image/pjpeg", "image/jpeg" },
            { "image/x-png", "image/png" },
            { "image/vnd.rim.png", "image/png" },
            { "image/ico", "image/vnd.microsoft.icon" },
            { "image/icon", "image/vnd.microsoft.icon" },
            { "text/ico", "image/vnd.microsoft.icon" },
            { "application/ico", "image/vnd.microsoft.icon" },
            { "image/x-icon", "image/vnd.microsoft.icon" },
            { "audio/vnd.qcelp", "audio/qcelp" },
            { "audio/qcp", "audio/qcelp" },
            { "audio/vnd.qcp", "audio/qcelp" },
            { "audio/wav", "audio/x-wav" },
            { "audio/vnd.wave", "audio/x-wav" },
            { "audio/mid", "audio/midi" },
            { "audio/sp-midi", "audio/midi" },
            { "audio/x-mid", "audio/midi" },
            { "audio/x-midi", "audio/midi" },
            { "audio/x-mpeg", "audio/mpeg" },
            { "audio/mp3", "audio/mpeg" },
            { "audio/x-mp3", "audio/mpeg" },
            { "audio/mpeg3", "audio/mpeg" },
            { "audio/x-mpeg3", "audio/mpeg" },
            { "audio/mpg3", "audio/mpeg" },
            { "audio/mpg", "audio/mpeg" },
            { "audio/x-mpg", "audio/mpeg" },
            { "audio/m4a", "audio/mp4" },
            { "audio/x-m4a", "audio/mp4" },
            { "audio/x-mp4", "audio/mp4" },
            { "audio/x-aac", "audio/aac" },
            { "audio/x-amr", "audio/amr" },
            { "audio/mpegurl", "audio/x-mpegurl" },
            { "audio/flac", "audio/x-flac" },
            { "video/3gp", "video/3gpp" },
            { "video/avi", "video/x-msvideo" },
            { "video/x-m4v", "video/mp4" },
            { "video/x-quicktime", "video/quicktime" },
            { "application/java", "application/java-archive" },
            { "application/x-java-archive", "application/java-archive" },
            { "application/x-zip-compressed", "application/zip" },
            { "text/cache-manifest", "text/plain" },
        };

        HashMap<String, String, ASCIICaseInsensitiveHash> map;
        for (auto& pair : mimeTypeAssociations)
            map.add(ASCIILiteral { pair.first }, ASCIILiteral { pair.second });
        return map;
    }());

    auto it = mimeTypeAssociationMap.get().find(mimeType);
    if (it != mimeTypeAssociationMap.get().end())
        return it->value;
    return mimeType;
}

#endif

String MIMETypeRegistry::appendFileExtensionIfNecessary(const String& filename, const String& mimeType)
{
    if (filename.isEmpty())
        return emptyString();

    if (filename.reverseFind('.') != notFound)
        return filename;

    String preferredExtension = getPreferredExtensionForMIMEType(mimeType);
    if (preferredExtension.isEmpty())
        return filename;

    return filename + "." + preferredExtension;
}

} // namespace WebCore
