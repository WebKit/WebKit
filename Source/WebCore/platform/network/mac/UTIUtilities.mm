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

#import <wtf/Lock.h>
#import <wtf/MainThread.h>
#import <wtf/SortedArrayMap.h>
#import <wtf/TinyLRUCache.h>
#import <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)
#import <MobileCoreServices/MobileCoreServices.h>
#endif

namespace WebCore {

String MIMETypeFromUTI(const String& uti)
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return adoptCF(UTTypeCopyPreferredTagWithClass(uti.createCFString().get(), kUTTagClassMIMEType)).get();
ALLOW_DEPRECATED_DECLARATIONS_END
}

String MIMETypeFromUTITree(const String& uti)
{
    auto utiCF = uti.createCFString();

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // Check if this UTI has a MIME type.
    RetainPtr<CFStringRef> mimeType = adoptCF(UTTypeCopyPreferredTagWithClass(utiCF.get(), kUTTagClassMIMEType));
    if (mimeType)
        return mimeType.get();

    // If not, walk the ancestory of this UTI via its "ConformsTo" tags and return the first MIME type we find.
    RetainPtr<CFDictionaryRef> decl = adoptCF(UTTypeCopyDeclaration(utiCF.get()));
    if (!decl)
        return emptyString();
    CFTypeRef value = CFDictionaryGetValue(decl.get(), kUTTypeConformsToKey);
ALLOW_DEPRECATED_DECLARATIONS_END
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

static CFStringRef UTIFromUnknownMIMEType(StringView mimeType)
{
    static constexpr std::pair<ComparableLettersLiteral, NSString *> typesArray[] = {
        { "model/usd", @"com.pixar.universal-scene-description-mobile" },
        { "model/vnd.pixar.usd", @"com.pixar.universal-scene-description-mobile" },
        { "model/vnd.reality", @"com.apple.reality" },
        { "model/vnd.usdz+zip", @"com.pixar.universal-scene-description-mobile" },
    };
    static constexpr SortedArrayMap typesMap { typesArray };
    return (__bridge CFStringRef)typesMap.get(mimeType, @"");
}

struct UTIFromMIMETypeCachePolicy : TinyLRUCachePolicy<String, RetainPtr<CFStringRef>> {
public:
    static RetainPtr<CFStringRef> createValueForKey(const String& mimeType)
    {
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        auto type = adoptCF(UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType, mimeType.createCFString().get(), 0));
ALLOW_DEPRECATED_DECLARATIONS_END
        if (type)
            return type;
        return UTIFromUnknownMIMEType(mimeType);
    }

    static String createKeyForStorage(const String& key) { return key.isolatedCopy(); }
};

static TinyLRUCache<String, RetainPtr<CFStringRef>, 16, UTIFromMIMETypeCachePolicy>& cacheUTIFromMimeType()
{
    static NeverDestroyed<TinyLRUCache<String, RetainPtr<CFStringRef>, 16, UTIFromMIMETypeCachePolicy>> cache;
    return cache;
}

String UTIFromMIMEType(const String& mimeType)
{
    static Lock lock;
    auto locker = holdLock(lock);
    return cacheUTIFromMimeType().get(mimeType).get();
}

bool isDeclaredUTI(const String& UTI)
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return UTTypeIsDeclared(UTI.createCFString().get());
ALLOW_DEPRECATED_DECLARATIONS_END
}

String UTIFromTag(const String& tagClass, const String& tag, const String& conformingToUTI)
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    auto u = adoptCF(UTTypeCreatePreferredIdentifierForTag(tagClass.createCFString().get(), tag.createCFString().get(), conformingToUTI.createCFString().get()));
ALLOW_DEPRECATED_DECLARATIONS_END
    return String(u.get());
}

}
