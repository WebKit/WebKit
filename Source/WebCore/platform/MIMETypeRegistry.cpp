/*
 * Copyright (C) 2006-2022 Apple Inc. All rights reserved.
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

#include "DeprecatedGlobalSettings.h"
#include "MediaPlayer.h"
#include "ThreadGlobalData.h"
#include <wtf/FixedVector.h>
#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SortedArrayMap.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

#if USE(CG)
#include "ImageBufferUtilitiesCG.h"
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

#if USE(GSTREAMER) && ENABLE(VIDEO)
#include "ImageDecoderGStreamer.h"
#endif

namespace WebCore {

static String normalizedImageMIMEType(const String&);

// On iOS, we include malformed image MIME types for compatibility with Mail.
// These were removed for <rdar://problem/6564538> Re-enable UTI code in WebCore now that
// MobileCoreServices exists. But Mail relies on at least image/tif reported as being
// supported (should be image/tiff). This can be removed when Mail addresses:
// <rdar://problem/7879510> Mail should use standard image mimetypes
// and we fix sniffing so that it corrects items such as image/jpg -> image/jpeg.
constexpr ComparableCaseFoldingASCIILiteral supportedImageMIMETypeArray[] = {
#if PLATFORM(IOS_FAMILY)
    "application/bmp",
    "application/jpg",
    "application/png",
    "application/tif",
    "application/tiff",
    "application/x-bmp",
    "application/x-jpg",
    "application/x-png",
    "application/x-tif",
    "application/x-tiff",
    "application/x-win-bitmap",
#endif
#if USE(CG) || ENABLE(APNG)
    "image/apng",
#endif
#if HAVE(AVIF) || USE(AVIF)
    "image/avif",
#endif
    "image/bmp",
#if PLATFORM(IOS_FAMILY)
    "image/gi_",
#endif
    "image/gif",
#if USE(CG) || USE(OPENJPEG)
    "image/jp2",
#endif
#if PLATFORM(IOS_FAMILY)
    "image/jp_",
    "image/jpe_",
#endif
    "image/jpeg",
#if !USE(CG) && USE(OPENJPEG)
    "image/jpeg2000",
#endif
    "image/jpg",
#if USE(JPEGXL)
    "image/jxl",
#endif
#if PLATFORM(IOS_FAMILY)
    "image/ms-bmp",
    "image/pipeg",
#endif
#if USE(CG)
    "image/pjpeg",
#endif
    "image/png",
#if PLATFORM(IOS_FAMILY)
    "image/tif",
#endif
#if USE(CG)
    "image/tiff",
#endif
    "image/vnd.microsoft.icon",
#if PLATFORM(IOS_FAMILY)
    "image/vnd.switfview-jpeg",
#endif
#if HAVE(WEBP) || USE(WEBP)
    "image/webp",
#endif
#if PLATFORM(IOS_FAMILY)
    "image/x-bmp",
#endif
    "image/x-icon",
#if PLATFORM(IOS_FAMILY)
    "image/x-ms-bmp",
    "image/x-tif",
    "image/x-tiff",
    "image/x-win-bitmap",
    "image/x-windows-bmp",
#endif
#if PLATFORM(IOS_FAMILY) || !USE(CG)
    "image/x-xbitmap",
#endif
};

template<ASCIISubset subset, unsigned size> static FixedVector<ASCIILiteral> makeFixedVector(const ComparableASCIISubsetLiteral<subset> (&array)[size])
{
    FixedVector<ASCIILiteral> result(std::size(array));
    std::transform(std::begin(array), std::end(array), result.begin(), [] (auto literal) {
        return literal.literal;
    });
    return result;
}

FixedVector<ASCIILiteral> MIMETypeRegistry::supportedImageMIMETypes()
{
    return makeFixedVector(supportedImageMIMETypeArray);
}

HashSet<String, ASCIICaseInsensitiveHash>& MIMETypeRegistry::additionalSupportedImageMIMETypes()
{
    static NeverDestroyed<HashSet<String, ASCIICaseInsensitiveHash>> additionalSupportedImageMIMETypes;
    return additionalSupportedImageMIMETypes;
}

// https://html.spec.whatwg.org/multipage/scripting.html#javascript-mime-type
constexpr ComparableLettersLiteral supportedJavaScriptMIMETypeArray[] = {
    "application/ecmascript",
    "application/javascript",
    "application/x-ecmascript",
    "application/x-javascript",
    "text/ecmascript",
    "text/javascript",
    "text/javascript1.0",
    "text/javascript1.1",
    "text/javascript1.2",
    "text/javascript1.3",
    "text/javascript1.4",
    "text/javascript1.5",
    "text/jscript",
    "text/livescript",
    "text/x-ecmascript",
    "text/x-javascript",
};

HashSet<String, ASCIICaseInsensitiveHash>& MIMETypeRegistry::supportedNonImageMIMETypes()
{
    static NeverDestroyed types = [] {
        HashSet<String, ASCIICaseInsensitiveHash> types = std::initializer_list<String> {
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
        for (auto& type : supportedJavaScriptMIMETypeArray)
            types.add(type.literal);
#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
        ArchiveFactory::registerKnownArchiveMIMETypes(types);
#endif
        return types;
    }();
    return types;
}

const HashSet<String, ASCIICaseInsensitiveHash>& MIMETypeRegistry::supportedMediaMIMETypes()
{
    static NeverDestroyed types = [] {
        HashSet<String, ASCIICaseInsensitiveHash> types;
#if ENABLE(VIDEO)
        MediaPlayer::getSupportedTypes(types);
#endif
        return types;
    }();
    return types;
}

constexpr ComparableLettersLiteral pdfMIMETypeArray[] = {
    "application/pdf",
    "text/pdf",
};

FixedVector<ASCIILiteral> MIMETypeRegistry::pdfMIMETypes()
{
    return makeFixedVector(pdfMIMETypeArray);
}

constexpr ComparableLettersLiteral unsupportedTextMIMETypeArray[] = {
    "text/calendar",
    "text/directory",
    "text/ldif",
    "text/qif",
#if !PLATFORM(IOS_FAMILY)
    "text/rtf",
#endif
    "text/vcalendar",
    "text/vcard",
#if PLATFORM(IOS_FAMILY)
    "text/vnd.sun.j2me.app-descriptor",
#endif
    "text/x-calendar",
    "text/x-csv",
    "text/x-qif",
    "text/x-vcalendar",
    "text/x-vcard",
    "text/x-vcf",
};

FixedVector<ASCIILiteral> MIMETypeRegistry::unsupportedTextMIMETypes()
{
    return makeFixedVector(unsupportedTextMIMETypeArray);
}

static const HashMap<String, Vector<String>, ASCIICaseInsensitiveHash>& commonMimeTypesMap()
{
    ASSERT(isMainThread());
    static NeverDestroyed<HashMap<String, Vector<String>, ASCIICaseInsensitiveHash>> mimeTypesMap = [] {
        HashMap<String, Vector<String>, ASCIICaseInsensitiveHash> map;
        // A table of common media MIME types and file extensions used when a platform's
        // specific MIME type lookup doesn't have a match for a media file extension.
        static constexpr TypeExtensionPair commonMediaTypes[] = {
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
        for (auto& pair : commonMediaTypes) {
            ASCIILiteral type = pair.type;
            ASCIILiteral extension = pair.extension;
            map.ensure(extension, [type, extension] {
                // First type in the vector must always be the one from mimeTypeForExtension,
                // so we can use the map without also calling mimeTypeForExtension each time.
                Vector<String> synonyms;
                String systemType = MIMETypeRegistry::mimeTypeForExtension(StringView(extension));
                if (!systemType.isEmpty() && type != systemType)
                    synonyms.append(systemType);
                return synonyms;
            }).iterator->value.append(type);
        }
        return map;
    }();
    return mimeTypesMap;
}

static const Vector<String>* typesForCommonExtension(StringView extension)
{
    auto mapEntry = commonMimeTypesMap().find<ASCIICaseInsensitiveStringViewHashTranslator>(extension);
    if (mapEntry == commonMimeTypesMap().end())
        return nullptr;
    return &mapEntry->value;
}

String MIMETypeRegistry::mediaMIMETypeForExtension(StringView extension)
{
    auto* vector = typesForCommonExtension(extension);
    if (vector)
        return (*vector)[0];
    return mimeTypeForExtension(extension);
}

String MIMETypeRegistry::mimeTypeForPath(StringView path)
{
    auto position = path.reverseFind('.');
    if (position != notFound) {
        auto result = mimeTypeForExtension(path.substring(position + 1));
        if (result.length())
            return result;
    }
    return defaultMIMEType();
}

bool MIMETypeRegistry::isSupportedImageMIMEType(const String& mimeType)
{
    if (mimeType.isEmpty())
        return false;
    static constexpr SortedArraySet supportedImageMIMETypeSet { supportedImageMIMETypeArray };
#if USE(CG) && ASSERT_ENABLED
    // Ensure supportedImageMIMETypeArray matches defaultSupportedImageTypes().
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        for (auto& imageType : defaultSupportedImageTypes()) {
            auto mimeType = MIMETypeForImageType(imageType);
            ASSERT_IMPLIES(!mimeType.isEmpty(), supportedImageMIMETypeSet.contains(mimeType));
        }
    });
#endif

    String normalizedMIMEType = normalizedImageMIMEType(mimeType);
    if (supportedImageMIMETypeSet.contains(normalizedMIMEType))
        return true;
    return additionalSupportedImageMIMETypes().contains(normalizedMIMEType);
}

bool MIMETypeRegistry::isSupportedImageVideoOrSVGMIMEType(const String& mimeType)
{
    if (isSupportedImageMIMEType(mimeType) || equalLettersIgnoringASCIICase(mimeType, "image/svg+xml"_s))
        return true;

#if HAVE(AVASSETREADER)
    if (ImageDecoderAVFObjC::supportsContainerType(mimeType))
        return true;
#endif

#if USE(GSTREAMER) && ENABLE(VIDEO)
    if (ImageDecoderGStreamer::supportsContainerType(mimeType))
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
        if (!isSupportedImageType(supportedType))
            continue;
        String mimeType = MIMETypeForImageType(supportedType);
        if (mimeType.isEmpty())
            continue;
        supportedImageMIMETypesForEncoding.add(mimeType);
    }
#else
    HashSet<String, ASCIICaseInsensitiveHash> supportedImageMIMETypesForEncoding = std::initializer_list<String> {
#if USE(CG)
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
    static constexpr SortedArraySet supportedJavaScriptMIMETypes { supportedJavaScriptMIMETypeArray };
    return supportedJavaScriptMIMETypes.contains(mimeType);
}

bool MIMETypeRegistry::isSupportedWebAssemblyMIMEType(const String& mimeType)
{
    return equalLettersIgnoringASCIICase(mimeType, "application/wasm"_s);
}

bool MIMETypeRegistry::isSupportedStyleSheetMIMEType(const String& mimeType)
{
    return equalLettersIgnoringASCIICase(mimeType, "text/css"_s);
}

bool MIMETypeRegistry::isSupportedFontMIMEType(const String& mimeType)
{
    static const unsigned fontLength = 5;
    if (!startsWithLettersIgnoringASCIICase(mimeType, "font/"_s))
        return false;
    auto subtype = StringView { mimeType }.substring(fontLength);
    return equalLettersIgnoringASCIICase(subtype, "woff"_s)
        || equalLettersIgnoringASCIICase(subtype, "woff2"_s)
        || equalLettersIgnoringASCIICase(subtype, "otf"_s)
        || equalLettersIgnoringASCIICase(subtype, "ttf"_s)
        || equalLettersIgnoringASCIICase(subtype, "sfnt"_s);
}

bool MIMETypeRegistry::isTextMediaPlaylistMIMEType(const String& mimeType)
{
    if (startsWithLettersIgnoringASCIICase(mimeType, "application/"_s)) {
        static const unsigned applicationLength = 12;
        auto subtype = StringView { mimeType }.substring(applicationLength);
        return equalLettersIgnoringASCIICase(subtype, "vnd.apple.mpegurl"_s)
            || equalLettersIgnoringASCIICase(subtype, "mpegurl"_s)
            || equalLettersIgnoringASCIICase(subtype, "x-mpegurl"_s);
    }

    if (startsWithLettersIgnoringASCIICase(mimeType, "audio/"_s)) {
        static const unsigned audioLength = 6;
        auto subtype = StringView { mimeType }.substring(audioLength);
        return equalLettersIgnoringASCIICase(subtype, "mpegurl"_s)
            || equalLettersIgnoringASCIICase(subtype, "x-mpegurl"_s);
    }

    return false;
}

// https://mimesniff.spec.whatwg.org/#json-mime-type
bool MIMETypeRegistry::isSupportedJSONMIMEType(const String& mimeType)
{
    if (mimeType.isEmpty())
        return false;

    if (equalLettersIgnoringASCIICase(mimeType, "application/json"_s))
        return true;

    if (equalLettersIgnoringASCIICase(mimeType, "text/json"_s))
        return true;

    // When detecting +json ensure there is a non-empty type / subtype preceeding the suffix.
    if (mimeType.endsWithIgnoringASCIICase("+json"_s) && mimeType.length() >= 8) {
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
    return equalLettersIgnoringASCIICase(mimeType, "text/vtt"_s);
}

bool MIMETypeRegistry::isUnsupportedTextMIMEType(const String& mimeType)
{
    static constexpr SortedArraySet unsupportedTextMIMETypes { unsupportedTextMIMETypeArray };
    return unsupportedTextMIMETypes.contains(mimeType);
}

bool MIMETypeRegistry::isTextMIMEType(const String& mimeType)
{
    return isSupportedJavaScriptMIMEType(mimeType)
        || isSupportedJSONMIMEType(mimeType) // Render JSON as text/plain.
        || (startsWithLettersIgnoringASCIICase(mimeType, "text/"_s)
            && !equalLettersIgnoringASCIICase(mimeType, "text/html"_s)
            && !equalLettersIgnoringASCIICase(mimeType, "text/xml"_s)
            && !equalLettersIgnoringASCIICase(mimeType, "text/xsl"_s));
}

static inline bool isValidXMLMIMETypeChar(UChar c)
{
    // Valid characters per RFCs 3023 and 2045: 0-9a-zA-Z_-+~!$^{}|.%'`#&*
    return isASCIIAlphanumeric(c) || c == '!' || c == '#' || c == '$' || c == '%' || c == '&' || c == '\'' || c == '*' || c == '+'
        || c == '-' || c == '.' || c == '^' || c == '_' || c == '`' || c == '{' || c == '|' || c == '}' || c == '~';
}

bool MIMETypeRegistry::isXMLMIMEType(const String& mimeType)
{
    if (equalLettersIgnoringASCIICase(mimeType, "text/xml"_s) || equalLettersIgnoringASCIICase(mimeType, "application/xml"_s) || equalLettersIgnoringASCIICase(mimeType, "text/xsl"_s))
        return true;

    if (!mimeType.endsWithIgnoringASCIICase("+xml"_s))
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

bool MIMETypeRegistry::isXMLEntityMIMEType(StringView mimeType)
{
    return equalLettersIgnoringASCIICase(mimeType, "text/xml-external-parsed-entity"_s)
        || equalLettersIgnoringASCIICase(mimeType, "application/xml-external-parsed-entity"_s);
}

bool MIMETypeRegistry::isJavaAppletMIMEType(const String& mimeType)
{
    // Since this set is very limited and is likely to remain so we won't bother with the overhead
    // of using a hash set.
    // Any of the MIME types below may be followed by any number of specific versions of the JVM,
    // which is why we use startsWith()
    return startsWithLettersIgnoringASCIICase(mimeType, "application/x-java-applet"_s)
        || startsWithLettersIgnoringASCIICase(mimeType, "application/x-java-bean"_s)
        || startsWithLettersIgnoringASCIICase(mimeType, "application/x-java-vm"_s);
}

bool MIMETypeRegistry::isPDFMIMEType(const String& mimeType)
{
    static constexpr SortedArraySet set { pdfMIMETypeArray };
    return set.contains(mimeType);
}

bool MIMETypeRegistry::isPostScriptMIMEType(const String& mimeType)
{
    return equalLettersIgnoringASCIICase(mimeType, "application/postscript"_s);
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

#if ENABLE(MODEL_ELEMENT)
    if (isSupportedModelMIMEType(mimeType) && DeprecatedGlobalSettings::modelDocumentEnabled())
        return true;
#endif

    if (startsWithLettersIgnoringASCIICase(mimeType, "text/"_s))
        return !isUnsupportedTextMIMEType(mimeType);

    return false;
}

const String& defaultMIMEType()
{
    static NeverDestroyed<const String> defaultMIMEType(MAKE_STATIC_STRING_IMPL("application/octet-stream"));
    return defaultMIMEType;
}

constexpr ComparableLettersLiteral usdMIMETypeArray[] = {
    "model/usd", // Unofficial, but supported because we documented this.
    "model/vnd.pixar.usd", // Unofficial, but supported because we documented this.
    "model/vnd.reality",
    "model/vnd.usdz+zip", // The official type: https://www.iana.org/assignments/media-types/model/vnd.usdz+zip
};

FixedVector<ASCIILiteral> MIMETypeRegistry::usdMIMETypes()
{
    return makeFixedVector(usdMIMETypeArray);
}

bool MIMETypeRegistry::isUSDMIMEType(const String& mimeType)
{
    static constexpr SortedArraySet usdMIMETypeSet { usdMIMETypeArray };
    return usdMIMETypeSet.contains(mimeType);
}

bool MIMETypeRegistry::isSupportedModelMIMEType(const String& mimeType)
{
    return MIMETypeRegistry::isUSDMIMEType(mimeType);
}

// FIXME: Not great that CURL needs this concept; other platforms do not.
static String normalizedImageMIMEType(const String& mimeType)
{
#if USE(CURL)
    // FIXME: Since this is only used in isSupportedImageMIMEType, we should consider removing the non-image types below.
    static constexpr std::pair<ComparableLettersLiteral, ASCIILiteral> mimeTypeAssociationArray[] = {
        { "application/ico", "image/vnd.microsoft.icon"_s },
        { "application/java", "application/java-archive"_s },
        { "application/x-java-archive", "application/java-archive"_s },
        { "application/x-zip-compressed", "application/zip"_s },
        { "audio/flac", "audio/x-flac"_s },
        { "audio/m4a", "audio/mp4"_s },
        { "audio/mid", "audio/midi"_s },
        { "audio/mp3", "audio/mpeg"_s },
        { "audio/mpeg3", "audio/mpeg"_s },
        { "audio/mpegurl", "audio/x-mpegurl"_s },
        { "audio/mpg", "audio/mpeg"_s },
        { "audio/mpg3", "audio/mpeg"_s },
        { "audio/qcp", "audio/qcelp"_s },
        { "audio/sp-midi", "audio/midi"_s },
        { "audio/vnd.qcelp", "audio/qcelp"_s },
        { "audio/vnd.qcp", "audio/qcelp"_s },
        { "audio/vnd.wave", "audio/x-wav"_s },
        { "audio/wav", "audio/x-wav"_s },
        { "audio/x-aac", "audio/aac"_s },
        { "audio/x-amr", "audio/amr"_s },
        { "audio/x-m4a", "audio/mp4"_s },
        { "audio/x-mid", "audio/midi"_s },
        { "audio/x-midi", "audio/midi"_s },
        { "audio/x-mp3", "audio/mpeg"_s },
        { "audio/x-mp4", "audio/mp4"_s },
        { "audio/x-mpeg", "audio/mpeg"_s },
        { "audio/x-mpeg3", "audio/mpeg"_s },
        { "audio/x-mpg", "audio/mpeg"_s },
        { "image/ico", "image/vnd.microsoft.icon"_s },
        { "image/icon", "image/vnd.microsoft.icon"_s },
        { "image/jpg", "image/jpeg"_s },
        { "image/pjpeg", "image/jpeg"_s },
        { "image/vnd.rim.png", "image/png"_s },
        { "image/x-bitmap", "image/bmp"_s },
        { "image/x-bmp", "image/bmp"_s },
        { "image/x-icon", "image/vnd.microsoft.icon"_s },
        { "image/x-ms-bitmap", "image/bmp"_s },
        { "image/x-ms-bmp", "image/bmp"_s },
        { "image/x-png", "image/png"_s },
        { "image/x-windows-bmp", "image/bmp"_s },
        { "text/cache-manifest", "text/plain"_s },
        { "text/ico", "image/vnd.microsoft.icon"_s },
        { "video/3gp", "video/3gpp"_s },
        { "video/avi", "video/x-msvideo"_s },
        { "video/x-m4v", "video/mp4"_s },
        { "video/x-quicktime", "video/quicktime"_s },
    };
    static constexpr SortedArrayMap associationMap { mimeTypeAssociationArray };
    auto normalizedType = associationMap.tryGet(mimeType);
    return normalizedType ? *normalizedType : mimeType;
#else
    return mimeType;
#endif
}

String MIMETypeRegistry::appendFileExtensionIfNecessary(const String& filename, const String& mimeType)
{
    if (filename.isEmpty() || filename.contains('.') || equalIgnoringASCIICase(mimeType, defaultMIMEType()))
        return filename;

    auto preferredExtension = preferredExtensionForMIMEType(mimeType);
    if (preferredExtension.isEmpty())
        return filename;

    return makeString(filename, '.', preferredExtension);
}

static inline String trimmedExtension(const String& extension)
{
    return extension.startsWith('.') ? extension.right(extension.length() - 1) : extension;
}

String MIMETypeRegistry::preferredImageMIMETypeForEncoding(const Vector<String>& mimeTypes, const Vector<String>& extensions)
{
    auto allowedMIMETypes = MIMETypeRegistry::allowedMIMETypes(mimeTypes, extensions);

    auto position = allowedMIMETypes.findIf([](const auto& mimeType) {
        return MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType);
    });
    
    return position != notFound ? allowedMIMETypes[position] : nullString();
}

bool MIMETypeRegistry::containsImageMIMETypeForEncoding(const Vector<String>& mimeTypes, const Vector<String>& extensions)
{
    return !MIMETypeRegistry::preferredImageMIMETypeForEncoding(mimeTypes, extensions).isNull();
}

Vector<String> MIMETypeRegistry::allowedMIMETypes(const Vector<String>& mimeTypes, const Vector<String>& extensions)
{
    Vector<String> allowedMIMETypes;

    for (auto& mimeType : mimeTypes)
        allowedMIMETypes.appendIfNotContains(mimeType.convertToASCIILowercase());

    for (auto& extension : extensions) {
        auto mimeType = MIMETypeRegistry::mimeTypeForExtension(trimmedExtension(extension));
        if (mimeType.isEmpty())
            continue;
        allowedMIMETypes.appendIfNotContains(mimeType.convertToASCIILowercase());
    }

    return allowedMIMETypes;
}

Vector<String> MIMETypeRegistry::allowedFileExtensions(const Vector<String>& mimeTypes, const Vector<String>& extensions)
{
    Vector<String> allowedFileExtensions;

    for (auto& mimeType : mimeTypes) {
        for (auto& extension : MIMETypeRegistry::extensionsForMIMEType(mimeType))
            allowedFileExtensions.appendIfNotContains(extension);
    }

    for (auto& extension : extensions)
        allowedFileExtensions.appendIfNotContains(trimmedExtension(extension));

    return allowedFileExtensions;
}

bool MIMETypeRegistry::isJPEGMIMEType(const String& mimeType)
{
#if USE(CG)
    auto destinationUTI = utiFromImageBufferMIMEType(mimeType);
    if (!destinationUTI)
        return false;
    return CFEqual(destinationUTI.get(), jpegUTI());
#else
    return mimeType == "image/jpeg"_s || mimeType == "image/jpg"_s;
#endif
}

} // namespace WebCore
