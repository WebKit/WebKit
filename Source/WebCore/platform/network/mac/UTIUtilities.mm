/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "UTIUtilities.h"
#import <wtf/MainThread.h>
#import <wtf/TinyLRUCache.h>
#import <wtf/text/StringHash.h>
#import <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)
#import <MobileCoreServices/MobileCoreServices.h>
#endif

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/AdditionalUTIMappings.h>
#else
#define ADDITIONAL_UTI_MAPPINGS
#endif

#define EMPTY_MIME_TYPE_STRING "emptyMimeType"_s

namespace WebCore {

String MIMETypeFromUTI(const String& uti)
{
    return adoptCF(UTTypeCopyPreferredTagWithClass(uti.createCFString().get(), kUTTagClassMIMEType)).get();
}

String MIMETypeFromUTITree(const String& uti)
{
    auto utiCF = uti.createCFString();

    // Check if this UTI has a MIME type.
    RetainPtr<CFStringRef> mimeType = adoptCF(UTTypeCopyPreferredTagWithClass(utiCF.get(), kUTTagClassMIMEType));
    if (mimeType)
        return mimeType.get();

    // If not, walk the ancestory of this UTI via its "ConformsTo" tags and return the first MIME type we find.
    RetainPtr<CFDictionaryRef> decl = adoptCF(UTTypeCopyDeclaration(utiCF.get()));
    if (!decl)
        return emptyString();
    CFTypeRef value = CFDictionaryGetValue(decl.get(), kUTTypeConformsToKey);
    if (!value)
        return emptyString();
    CFTypeID typeID = CFGetTypeID(value);

    if (typeID == CFStringGetTypeID())
        return MIMETypeFromUTITree((CFStringRef)value);

    if (typeID == CFArrayGetTypeID()) {
        CFArrayRef newTypes = (CFArrayRef)value;
        CFIndex count = CFArrayGetCount(newTypes);
        for (CFIndex i = 0; i < count; ++i) {
            CFTypeRef object = CFArrayGetValueAtIndex(newTypes, i);
            if (CFGetTypeID(object) != CFStringGetTypeID())
                continue;

            String mimeType = MIMETypeFromUTITree((CFStringRef)object);
            if (!mimeType.isEmpty())
                return mimeType;
        }
    }

    return emptyString();
}

static String UTIFromUnknownMIMEType(const String& mimeType)
{
    static const auto map = makeNeverDestroyed([] {
        struct TypeExtensionPair {
            ASCIILiteral type;
            ASCIILiteral uti;
        };

        static const TypeExtensionPair pairs[] = {
            { "model/vnd.usdz+zip"_s, "com.pixar.universal-scene-description-mobile"_s },
            { "model/usd"_s, "com.pixar.universal-scene-description-mobile"_s },
            { "model/vnd.pixar.usd"_s, "com.pixar.universal-scene-description-mobile"_s },
            ADDITIONAL_UTI_MAPPINGS
        };

        HashMap<String, String, ASCIICaseInsensitiveHash> map;
        for (auto& pair : pairs)
            map.add(pair.type, pair.uti);
        return map;
    }());

    auto mapEntry = map.get().find(mimeType);
    if (mapEntry == map.get().end())
        return emptyString();

    return mapEntry->value;
}

static Optional<HashMap<String, String>>& mapUTIFromMIMEType()
{
    static NeverDestroyed<Optional<HashMap<String, String>>> map;
    return map;
}

struct UTIFromMIMETypeCachePolicy : TinyLRUCachePolicy<String, String> {
public:
    static String createValueForKey(const String& mimeType)
    {
        String key = mimeType;
        if (mapUTIFromMIMEType().hasValue()) {
            if (key.isEmpty())
                key = EMPTY_MIME_TYPE_STRING;
            const auto& it = mapUTIFromMIMEType()->find(key);
            if (it != mapUTIFromMIMEType()->end())
                return it->value;
            WTFLogAlways("UTI for MIME type %s not found.", key.utf8().data());
            ASSERT_NOT_REACHED();
            return UTIFromUnknownMIMEType(key);
        }
        auto type = adoptCF(UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType, key.createCFString().get(), 0));
        if (type)
            return type.get();
        return UTIFromUnknownMIMEType(key);
    }
};

static TinyLRUCache<String, String, 16, UTIFromMIMETypeCachePolicy>& cacheUTIFromMimeType()
{
    static NeverDestroyed<TinyLRUCache<String, String, 16, UTIFromMIMETypeCachePolicy>> cache;
    return cache;
}

String UTIFromMIMEType(const String& mimeType)
{
    ASSERT(isMainThread());
    return cacheUTIFromMimeType().get(mimeType);
}

bool isDeclaredUTI(const String& UTI)
{
    return UTTypeIsDeclared(UTI.createCFString().get());
}

const Vector<String>& mimeTypes()
{
    static NeverDestroyed<Vector<String>> mimeTypes = std::initializer_list<String> {
        "application/ogg"_s,
        "audio/ogg"_s,
        "video/ogg"_s,
        "application/annodex"_s,
        "audio/annodex"_s,
        "video/annodex"_s,
        "audio/speex"_s,
        "video/webm"_s,
        "audio/webm"_s,
        "audio/mpeg"_s,
        "video/mpeg"_s,
        "application/vnd.apple.mpegurl"_s,
        "application/mpegurl"_s,
        "application/x-mpegurl"_s,
        "audio/mpegurl"_s,
        "audio/x-mpegurl"_s,
        "audio/mpegurl"_s,
        "video/x-m4v"_s,
        "audio/x-m4a"_s,
        "audio/x-m4b"_s,
        "audio/x-m4p"_s,
        "video/mp4"_s,
        "audio/mp4"_s,
        "audio/mp3"_s,
        "audio/x-mp3"_s,
        "audio/x-mpeg"_s,
        "video/x-mpeg2"_s,
        "video/mpeg2"_s,
        "video/m2ts"_s,
        "video/x-m2ts"_s,
        "audio/3gpp"_s,
        "audio/3gpp2"_s,
        "application/x-mpeg"_s,
        "audio/aac"_s,
        "audio/x-aac"_s,
        "audio/x-caf"_s,
        "audio/x-gsm"_s,
        "audio/x-wav"_s,
        "audio/vnd.wave"_s,
        "audio/wav"_s,

        "image/gif"_s,
        "image/jpeg"_s,
        "image/png"_s,

        "text/html"_s,
        "text/plain"_s,

        "application/pdf"_s,
        "multipart/x-folder"_s,
        "public.html"_s,
    };
    return mimeTypes;
};

const HashMap<String, String>& createUTIFromMIMETypeMap()
{
    static NeverDestroyed<HashMap<String, String>> map = [] {
        HashMap<String, String> cache;
        for (auto mimeType : mimeTypes())
            cache.add(mimeType, UTIFromMIMEType(mimeType));
        cache.add(EMPTY_MIME_TYPE_STRING, UTIFromMIMEType(emptyString()));
        return cache;
    }();
    return map;
}

void setUTIFromMIMETypeMap(HashMap<String, String>&& map)
{
    mapUTIFromMIMEType() = WTFMove(map);
}

}
