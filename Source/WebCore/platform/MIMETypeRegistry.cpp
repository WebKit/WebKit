/*
 * Copyright (C) 2006-2019 Apple Inc. All rights reserved.
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
#include "ThreadGlobalData.h"
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

#if USE(QUICK_LOOK)
#include "PreviewConverter.h"
#endif

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/AdditionalSystemPreviewTypes.h>
#else
#define ADDITIONAL_SYSTEM_PREVIEW_TYPES
#endif

namespace WebCore {

const HashSet<String, ASCIICaseInsensitiveHash>& MIMETypeRegistry::supportedImageMIMETypes()
{
    static NeverDestroyed<HashSet<String, ASCIICaseInsensitiveHash>> supportedImageMIMETypes = std::initializer_list<String> {
#if USE(CG)
        // This represents the subset of allowed image UTIs for which CoreServices has a corresponding MIME type.
        "image/tiff"_s,
        "image/gif"_s,
        "image/jpeg"_s,
        "image/vnd.microsoft.icon"_s,
        "image/jp2"_s,
        "image/apng"_s,
        "image/png"_s,
        "image/bmp"_s,

        "image/x-icon"_s, // Favicons don't have a MIME type in the registry either.
        "image/pjpeg"_s, //  We only get one MIME type per UTI, hence our need to add these manually

#if PLATFORM(IOS_FAMILY)
        // Add malformed image mimetype for compatibility with Mail and to handle malformed mimetypes from the net
        // These were removed for <rdar://problem/6564538> Re-enable UTI code in WebCore now that MobileCoreServices exists
        // But Mail relies on at least image/tif reported as being supported (should be image/tiff).
        // This can be removed when Mail addresses:
        // <rdar://problem/7879510> Mail should use standard image mimetypes
        // and we fix sniffing so that it corrects items such as image/jpg -> image/jpeg.

        // JPEG (image/jpeg)
        "image/jpg"_s,
        "image/jp_"_s,
        "image/jpe_"_s,
        "application/jpg"_s,
        "application/x-jpg"_s,
        "image/pipeg"_s,
        "image/vnd.switfview-jpeg"_s,
        "image/x-xbitmap"_s,

        // GIF (image/gif)
        "image/gi_"_s,

        // PNG (image/png)
        "application/png"_s,
        "application/x-png"_s,

        // TIFF (image/tiff)
        "image/x-tif"_s,
        "image/tif"_s,
        "image/x-tiff"_s,
        "application/tif"_s,
        "application/x-tif"_s,
        "application/tiff"_s,
        "application/x-tiff"_s,

        // BMP (image/bmp, image/x-bitmap)
        "image/x-bmp"_s,
        "image/x-win-bitmap"_s,
        "image/x-windows-bmp"_s,
        "image/ms-bmp"_s,
        "image/x-ms-bmp"_s,
        "application/bmp"_s,
        "application/x-bmp"_s,
        "application/x-win-bitmap"_s,
#endif
#else
        // assume that all implementations at least support the following standard
        // image types:
        "image/jpeg"_s,
        "image/png"_s,
        "image/gif"_s,
        "image/bmp"_s,
        "image/vnd.microsoft.icon"_s, // ico
        "image/x-icon"_s, // ico
        "image/x-xbitmap"_s, // xbm
#if ENABLE(APNG)
        "image/apng"_s,
#endif
#if USE(OPENJPEG)
        "image/jp2"_s,
        "image/jpeg2000"_s,
#endif
#if USE(WEBP)
        "image/webp"_s,
#endif
#endif
    };

#if USE(CG)
#ifndef NDEBUG
    // Esnure supportedImageMIMETypes() is in sync with defaultSupportedImageTypes().
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        for (auto& imageType : defaultSupportedImageTypes()) {
            auto mimeType = MIMETypeForImageType(imageType);
            ASSERT_IMPLIES(!mimeType.isEmpty(), supportedImageMIMETypes.get().contains(mimeType));
        }
    });
#endif
#endif
    return supportedImageMIMETypes;
}

HashSet<String, ASCIICaseInsensitiveHash>& MIMETypeRegistry::additionalSupportedImageMIMETypes()
{
    static NeverDestroyed<HashSet<String, ASCIICaseInsensitiveHash>> additionalSupportedImageMIMETypes;
    return additionalSupportedImageMIMETypes;
}

static const HashSet<String, ASCIICaseInsensitiveHash>& supportedJavaScriptMIMETypes()
{
    static NeverDestroyed<HashSet<String, ASCIICaseInsensitiveHash>> supportedJavaScriptMIMETypes = std::initializer_list<String> {
        // https://html.spec.whatwg.org/multipage/scripting.html#javascript-mime-type
        "text/javascript"_s,
        "text/ecmascript"_s,
        "application/javascript"_s,
        "application/ecmascript"_s,
        "application/x-javascript"_s,
        "application/x-ecmascript"_s,
        "text/javascript1.0"_s,
        "text/javascript1.1"_s,
        "text/javascript1.2"_s,
        "text/javascript1.3"_s,
        "text/javascript1.4"_s,
        "text/javascript1.5"_s,
        "text/jscript"_s,
        "text/livescript"_s,
        "text/x-javascript"_s,
        "text/x-ecmascript"_s,
    };
    return supportedJavaScriptMIMETypes;
}

HashSet<String, ASCIICaseInsensitiveHash>& MIMETypeRegistry::supportedNonImageMIMETypes()
{
    static auto supportedNonImageMIMETypes = makeNeverDestroyed([] {
        HashSet<String, ASCIICaseInsensitiveHash> supportedNonImageMIMETypes = std::initializer_list<String> {
            "text/html"_s,
            "text/xml"_s,
            "text/xsl"_s,
            "text/plain"_s,
            "text/"_s,
            "application/xml"_s,
            "application/xhtml+xml"_s,
#if !PLATFORM(IOS_FAMILY)
            "application/vnd.wap.xhtml+xml"_s,
            "application/rss+xml"_s,
            "application/atom+xml"_s,
#endif
            "application/json"_s,
            "image/svg+xml"_s,
#if ENABLE(FTPDIR)
            "application/x-ftp-directory"_s,
#endif
            "multipart/x-mixed-replace"_s,
        // Note: Adding a new type here will probably render it as HTML.
        // This can result in cross-site scripting vulnerabilities.
        };
        supportedNonImageMIMETypes.add(supportedJavaScriptMIMETypes().begin(), supportedJavaScriptMIMETypes().end());
#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
        ArchiveFactory::registerKnownArchiveMIMETypes(supportedNonImageMIMETypes);
#endif
        return supportedNonImageMIMETypes;
    }());
    return supportedNonImageMIMETypes;
}

const HashSet<String, ASCIICaseInsensitiveHash>& MIMETypeRegistry::supportedMediaMIMETypes()
{
    static const auto supportedMediaMIMETypes = makeNeverDestroyed([] {
        HashSet<String, ASCIICaseInsensitiveHash> supportedMediaMIMETypes;
#if ENABLE(VIDEO)
        MediaPlayer::getSupportedTypes(supportedMediaMIMETypes);
#endif
        return supportedMediaMIMETypes;
    }());
    return supportedMediaMIMETypes;
}

const HashSet<String, ASCIICaseInsensitiveHash>& MIMETypeRegistry::pdfMIMETypes()
{
    static NeverDestroyed<HashSet<String, ASCIICaseInsensitiveHash>> pdfMIMETypes = std::initializer_list<String> {
        "application/pdf"_s,
        "text/pdf"_s,
    };
    return pdfMIMETypes;
}

const HashSet<String, ASCIICaseInsensitiveHash>& MIMETypeRegistry::unsupportedTextMIMETypes()
{
    static NeverDestroyed<HashSet<String, ASCIICaseInsensitiveHash>> unsupportedTextMIMETypes = std::initializer_list<String> {
        "text/calendar"_s,
        "text/x-calendar"_s,
        "text/x-vcalendar"_s,
        "text/vcalendar"_s,
        "text/vcard"_s,
        "text/x-vcard"_s,
        "text/directory"_s,
        "text/ldif"_s,
        "text/qif"_s,
        "text/x-qif"_s,
        "text/x-csv"_s,
        "text/x-vcf"_s,
#if !PLATFORM(IOS_FAMILY)
        "text/rtf"_s,
#else
        "text/vnd.sun.j2me.app-descriptor"_s,
#endif
    };
    return unsupportedTextMIMETypes;
}

Optional<HashMap<String, Vector<String>, ASCIICaseInsensitiveHash>>& overriddenMimeTypesMap()
{
    static NeverDestroyed<Optional<HashMap<String, Vector<String>, ASCIICaseInsensitiveHash>>> map;
    return map;
}

const std::initializer_list<TypeExtensionPair>& commonMediaTypes()
{
    // A table of common media MIME types and file extensions used when a platform's
    // specific MIME type lookup doesn't have a match for a media file extension.
    static std::initializer_list<TypeExtensionPair> commonMediaTypes = {
        // Ogg
        { "application/ogg"_s, "ogx"_s },
        { "audio/ogg"_s, "ogg"_s },
        { "audio/ogg"_s, "oga"_s },
        { "video/ogg"_s, "ogv"_s },

        // Annodex
        { "application/annodex"_s, "anx"_s },
        { "audio/annodex"_s, "axa"_s },
        { "video/annodex"_s, "axv"_s },
        { "audio/speex"_s, "spx"_s },

        // WebM
        { "video/webm"_s, "webm"_s },
        { "audio/webm"_s, "webm"_s },

        // MPEG
        { "audio/mpeg"_s, "m1a"_s },
        { "audio/mpeg"_s, "m2a"_s },
        { "audio/mpeg"_s, "m1s"_s },
        { "audio/mpeg"_s, "mpa"_s },
        { "video/mpeg"_s, "mpg"_s },
        { "video/mpeg"_s, "m15"_s },
        { "video/mpeg"_s, "m1s"_s },
        { "video/mpeg"_s, "m1v"_s },
        { "video/mpeg"_s, "m75"_s },
        { "video/mpeg"_s, "mpa"_s },
        { "video/mpeg"_s, "mpeg"_s },
        { "video/mpeg"_s, "mpm"_s },
        { "video/mpeg"_s, "mpv"_s },

        // MPEG playlist
        { "application/vnd.apple.mpegurl"_s, "m3u8"_s },
        { "application/mpegurl"_s, "m3u8"_s },
        { "application/x-mpegurl"_s, "m3u8"_s },
        { "audio/mpegurl"_s, "m3url"_s },
        { "audio/x-mpegurl"_s, "m3url"_s },
        { "audio/mpegurl"_s, "m3u"_s },
        { "audio/x-mpegurl"_s, "m3u"_s },

        // MPEG-4
        { "video/x-m4v"_s, "m4v"_s },
        { "audio/x-m4a"_s, "m4a"_s },
        { "audio/x-m4b"_s, "m4b"_s },
        { "audio/x-m4p"_s, "m4p"_s },
        { "audio/mp4"_s, "m4a"_s },

        // MP3
        { "audio/mp3"_s, "mp3"_s },
        { "audio/x-mp3"_s, "mp3"_s },
        { "audio/x-mpeg"_s, "mp3"_s },

        // MPEG-2
        { "video/x-mpeg2"_s, "mp2"_s },
        { "video/mpeg2"_s, "vob"_s },
        { "video/mpeg2"_s, "mod"_s },
        { "video/m2ts"_s, "m2ts"_s },
        { "video/x-m2ts"_s, "m2t"_s },
        { "video/x-m2ts"_s, "ts"_s },

        // 3GP/3GP2
        { "audio/3gpp"_s, "3gpp"_s },
        { "audio/3gpp2"_s, "3g2"_s },
        { "application/x-mpeg"_s, "amc"_s },

        // AAC
        { "audio/aac"_s, "aac"_s },
        { "audio/aac"_s, "adts"_s },
        { "audio/x-aac"_s, "m4r"_s },

        // CoreAudio File
        { "audio/x-caf"_s, "caf"_s },
        { "audio/x-gsm"_s, "gsm"_s },

        // ADPCM
        { "audio/x-wav"_s, "wav"_s },
        { "audio/vnd.wave"_s, "wav"_s },
    };
    return commonMediaTypes;
}

const HashMap<String, Vector<String>, ASCIICaseInsensitiveHash>& commonMimeTypesMap()
{
    ASSERT(isMainThread());
    static NeverDestroyed<HashMap<String, Vector<String>, ASCIICaseInsensitiveHash>> mimeTypesMap = [] {
        HashMap<String, Vector<String>, ASCIICaseInsensitiveHash> map;
        for (auto& pair : commonMediaTypes()) {
            ASCIILiteral type = pair.type;
            ASCIILiteral extension = pair.extension;
            map.ensure(extension, [type, extension] {
                // First type in the vector must always be the one from getMIMETypeForExtension,
                // so we can use the map without also calling getMIMETypeForExtension each time.
                Vector<String> synonyms;
                String systemType = MIMETypeRegistry::getMIMETypeForExtension(extension);
                if (!systemType.isEmpty() && type != systemType)
                    synonyms.append(systemType);
                return synonyms;
            }).iterator->value.append(type);
        }
        return map;
    }();
    return mimeTypesMap;
}

static const Vector<String>* typesForCommonExtension(const String& extension)
{
    if (overriddenMimeTypesMap().hasValue()) {
        auto mapEntry = overriddenMimeTypesMap()->find(extension);
        if (mapEntry == overriddenMimeTypesMap()->end())
            return nullptr;
        return &mapEntry->value;
    }
    auto mapEntry = commonMimeTypesMap().find(extension);
    if (mapEntry == commonMimeTypesMap().end())
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
    String normalizedMIMEType = getNormalizedMIMEType(mimeType);
    return supportedImageMIMETypes().contains(normalizedMIMEType) || additionalSupportedImageMIMETypes().contains(normalizedMIMEType);
}

bool MIMETypeRegistry::isSupportedImageVideoOrSVGMIMEType(const String& mimeType)
{
    if (isSupportedImageMIMEType(mimeType) || equalLettersIgnoringASCIICase(mimeType, "image/svg+xml"))
        return true;

#if HAVE(AVASSETREADER)
    if (ImageDecoderAVFObjC::supportsContainerType(mimeType))
        return true;
#endif

    return false;
}

std::unique_ptr<MIMETypeRegistryThreadGlobalData> MIMETypeRegistry::createMIMETypeRegistryThreadGlobalData()
{
#if PLATFORM(COCOA)
    RetainPtr<CFArrayRef> supportedTypes = adoptCF(CGImageDestinationCopyTypeIdentifiers());
    HashSet<String, ASCIICaseInsensitiveHash> supportedImageMIMETypesForEncoding;
    CFIndex count = CFArrayGetCount(supportedTypes.get());
    for (CFIndex i = 0; i < count; i++) {
        CFStringRef supportedType = reinterpret_cast<CFStringRef>(CFArrayGetValueAtIndex(supportedTypes.get(), i));
        if (isSupportedImageType(supportedType)) {
            String mimeType = MIMETypeForImageType(supportedType);
            supportedImageMIMETypesForEncoding.add(mimeType);
        }
    }
#else
    HashSet<String, ASCIICaseInsensitiveHash> supportedImageMIMETypesForEncoding = std::initializer_list<String> {
#if USE(CG) || USE(DIRECT2D)
        // FIXME: Add Windows support for all the supported UTI's when a way to convert from MIMEType to UTI reliably is found.
        // For now, only support PNG, JPEG and GIF. See <rdar://problem/6095286>.
        "image/png"_s,
        "image/jpeg"_s,
        "image/gif"_s,
#elif PLATFORM(GTK)
        "image/png"_s,
        "image/jpeg"_s,
        "image/tiff"_s,
        "image/bmp"_s,
        "image/ico"_s,
#elif USE(CAIRO)
        "image/png"_s,
#endif
    };
#endif
    return makeUnique<MIMETypeRegistryThreadGlobalData>(WTFMove(supportedImageMIMETypesForEncoding));
}

bool MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(const String& mimeType)
{
    if (mimeType.isEmpty())
        return false;
    return threadGlobalData().mimeTypeRegistryThreadGlobalData().supportedImageMIMETypesForEncoding().contains(mimeType);
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

    return supportedJavaScriptMIMETypes().contains(mimeType);
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

bool MIMETypeRegistry::isTextMediaPlaylistMIMEType(const String& mimeType)
{
    if (startsWithLettersIgnoringASCIICase(mimeType, "application/")) {
        static const unsigned applicationLength = 12;
        auto subtype = StringView { mimeType }.substring(applicationLength);
        return equalLettersIgnoringASCIICase(subtype, "vnd.apple.mpegurl")
            || equalLettersIgnoringASCIICase(subtype, "mpegurl")
            || equalLettersIgnoringASCIICase(subtype, "x-mpegurl");
    }

    if (startsWithLettersIgnoringASCIICase(mimeType, "audio/")) {
        static const unsigned audioLength = 6;
        auto subtype = StringView { mimeType }.substring(audioLength);
        return equalLettersIgnoringASCIICase(subtype, "mpegurl")
            || equalLettersIgnoringASCIICase(subtype, "x-mpegurl");
    }

    return false;
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
    return supportedNonImageMIMETypes().contains(mimeType);
}

bool MIMETypeRegistry::isSupportedMediaMIMEType(const String& mimeType)
{
    if (mimeType.isEmpty())
        return false;
    return supportedMediaMIMETypes().contains(mimeType);
}

bool MIMETypeRegistry::isSupportedTextTrackMIMEType(const String& mimeType)
{
    return equalLettersIgnoringASCIICase(mimeType, "text/vtt");
}

bool MIMETypeRegistry::isUnsupportedTextMIMEType(const String& mimeType)
{
    if (mimeType.isEmpty())
        return false;
    return unsupportedTextMIMETypes().contains(mimeType);
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
    return pdfMIMETypes().contains(mimeType);
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

#if USE(QUICK_LOOK)
    if (PreviewConverter::supportsMIMEType(mimeType))
        return true;
#endif

    if (startsWithLettersIgnoringASCIICase(mimeType, "text/"))
        return !isUnsupportedTextMIMEType(mimeType);

    return false;
}

const String& defaultMIMEType()
{
    static NeverDestroyed<const String> defaultMIMEType(MAKE_STATIC_STRING_IMPL("application/octet-stream"));
    return defaultMIMEType;
}

const HashSet<String, ASCIICaseInsensitiveHash>& MIMETypeRegistry::systemPreviewMIMETypes()
{
    static NeverDestroyed<HashSet<String, ASCIICaseInsensitiveHash>> systemPreviewMIMETypes = std::initializer_list<String> {
        // The official type: https://www.iana.org/assignments/media-types/model/vnd.usdz+zip
        "model/vnd.usdz+zip",
        // Unofficial, but supported because we documented them.
        "model/usd",
        "model/vnd.pixar.usd",
        ADDITIONAL_SYSTEM_PREVIEW_TYPES
    };
    return systemPreviewMIMETypes;
}

bool MIMETypeRegistry::isSystemPreviewMIMEType(const String& mimeType)
{
    if (mimeType.isEmpty())
        return false;
    return systemPreviewMIMETypes().contains(mimeType);
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
        static const std::pair<ASCIILiteral, ASCIILiteral> mimeTypeAssociations[] = {
            { "image/x-ms-bmp"_s, "image/bmp"_s },
            { "image/x-windows-bmp"_s, "image/bmp"_s },
            { "image/x-bmp"_s, "image/bmp"_s },
            { "image/x-bitmap"_s, "image/bmp"_s },
            { "image/x-ms-bitmap"_s, "image/bmp"_s },
            { "image/jpg"_s, "image/jpeg"_s },
            { "image/pjpeg"_s, "image/jpeg"_s },
            { "image/x-png"_s, "image/png"_s },
            { "image/vnd.rim.png"_s, "image/png"_s },
            { "image/ico"_s, "image/vnd.microsoft.icon"_s },
            { "image/icon"_s, "image/vnd.microsoft.icon"_s },
            { "text/ico"_s, "image/vnd.microsoft.icon"_s },
            { "application/ico"_s, "image/vnd.microsoft.icon"_s },
            { "image/x-icon"_s, "image/vnd.microsoft.icon"_s },
            { "audio/vnd.qcelp"_s, "audio/qcelp"_s },
            { "audio/qcp"_s, "audio/qcelp"_s },
            { "audio/vnd.qcp"_s, "audio/qcelp"_s },
            { "audio/wav"_s, "audio/x-wav"_s },
            { "audio/vnd.wave"_s, "audio/x-wav"_s },
            { "audio/mid"_s, "audio/midi"_s },
            { "audio/sp-midi"_s, "audio/midi"_s },
            { "audio/x-mid"_s, "audio/midi"_s },
            { "audio/x-midi"_s, "audio/midi"_s },
            { "audio/x-mpeg"_s, "audio/mpeg"_s },
            { "audio/mp3"_s, "audio/mpeg"_s },
            { "audio/x-mp3"_s, "audio/mpeg"_s },
            { "audio/mpeg3"_s, "audio/mpeg"_s },
            { "audio/x-mpeg3"_s, "audio/mpeg"_s },
            { "audio/mpg3"_s, "audio/mpeg"_s },
            { "audio/mpg"_s, "audio/mpeg"_s },
            { "audio/x-mpg"_s, "audio/mpeg"_s },
            { "audio/m4a"_s, "audio/mp4"_s },
            { "audio/x-m4a"_s, "audio/mp4"_s },
            { "audio/x-mp4"_s, "audio/mp4"_s },
            { "audio/x-aac"_s, "audio/aac"_s },
            { "audio/x-amr"_s, "audio/amr"_s },
            { "audio/mpegurl"_s, "audio/x-mpegurl"_s },
            { "audio/flac"_s, "audio/x-flac"_s },
            { "video/3gp"_s, "video/3gpp"_s },
            { "video/avi"_s, "video/x-msvideo"_s },
            { "video/x-m4v"_s, "video/mp4"_s },
            { "video/x-quicktime"_s, "video/quicktime"_s },
            { "application/java"_s, "application/java-archive"_s },
            { "application/x-java-archive"_s, "application/java-archive"_s },
            { "application/x-zip-compressed"_s, "application/zip"_s },
            { "text/cache-manifest"_s, "text/plain"_s },
        };

        HashMap<String, String, ASCIICaseInsensitiveHash> map;
        for (auto& pair : mimeTypeAssociations)
            map.add(pair.first, pair.second);
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

    if (equalIgnoringASCIICase(mimeType, defaultMIMEType()))
        return filename;

    if (filename.reverseFind('.') != notFound)
        return filename;

    String preferredExtension = getPreferredExtensionForMIMEType(mimeType);
    if (preferredExtension.isEmpty())
        return filename;

    return makeString(filename, '.', preferredExtension);
}

} // namespace WebCore
