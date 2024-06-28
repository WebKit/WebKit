/*
 * Copyright (C) 2012-2024 Apple Inc. All rights reserved.
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

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <wtf/HashSet.h>
#import <wtf/Lock.h>
#import <wtf/MainThread.h>
#import <wtf/SortedArrayMap.h>
#import <wtf/TinyLRUCache.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/text/WTFString.h>
#include <wtf/cocoa/VectorCocoa.h>

#if PLATFORM(IOS_FAMILY)
#import <MobileCoreServices/MobileCoreServices.h>
#endif

#if HAVE(CGIMAGESOURCE_WITH_SET_ALLOWABLE_TYPES)
#include <pal/spi/cg/ImageIOSPI.h>
#endif

namespace WebCore {

String MIMETypeFromUTI(const String& uti)
{
    RetainPtr type = [UTType typeWithIdentifier:uti];
    return type.get().preferredMIMEType;
}

HashSet<String> RequiredMIMETypesFromUTI(const String& uti)
{
    HashSet<String> mimeTypes;

    auto mainMIMEType = MIMETypeFromUTI(uti);
    if (!mainMIMEType.isEmpty())
        mimeTypes.add(mainMIMEType);

    if (equalLettersIgnoringASCIICase(uti, "com.adobe.photoshop-image"_s))
        mimeTypes.add("application/x-photoshop"_s);

    return mimeTypes;
}

RetainPtr<CFStringRef> mimeTypeFromUTITree(CFStringRef uti)
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // Check if this UTI has a MIME type.
    if (auto type = adoptCF(UTTypeCopyPreferredTagWithClass(uti, kUTTagClassMIMEType)))
        return type;

    // If not, walk the ancestory of this UTI via its "ConformsTo" tags and return the first MIME type we find.
    auto declaration = adoptCF(UTTypeCopyDeclaration(uti));
    if (!declaration)
        return nullptr;

    auto value = CFDictionaryGetValue(declaration.get(), kUTTypeConformsToKey);
ALLOW_DEPRECATED_DECLARATIONS_END
    if (!value)
        return nullptr;

    if (auto string = dynamic_cf_cast<CFStringRef>(value))
        return mimeTypeFromUTITree(string);

    if (auto array = dynamic_cf_cast<CFArrayRef>(value)) {
        CFIndex count = CFArrayGetCount(array);
        for (CFIndex i = 0; i < count; ++i) {
            if (auto string = dynamic_cf_cast<CFStringRef>(CFArrayGetValueAtIndex(array, i))) {
                if (auto type = mimeTypeFromUTITree(string))
                    return type;
            }
        }
    }

    return nullptr;
}

static NSString *UTIFromUnknownMIMEType(StringView mimeType)
{
    static constexpr std::pair<ComparableLettersLiteral, NSString *> typesArray[] = {
        { "model/usd", @"com.pixar.universal-scene-description-mobile" },
        { "model/vnd.pixar.usd", @"com.pixar.universal-scene-description-mobile" },
        { "model/vnd.reality", @"com.apple.reality" },
        { "model/vnd.usdz+zip", @"com.pixar.universal-scene-description-mobile" },
    };
    static constexpr SortedArrayMap typesMap { typesArray };
    return typesMap.get(mimeType, @"");
}

struct UTIFromMIMETypeCachePolicy : TinyLRUCachePolicy<String, RetainPtr<NSString>> {
public:
    static RetainPtr<NSString> createValueForKey(const String& mimeType)
    {
        if (RetainPtr type = [UTType typeWithMIMEType:mimeType])
            return type.get().identifier;

        return UTIFromUnknownMIMEType(mimeType);
    }

    static String createKeyForStorage(const String& key) { return key.isolatedCopy(); }
};

static Lock cacheUTIFromMIMETypeLock;
static TinyLRUCache<String, RetainPtr<NSString>, 16, UTIFromMIMETypeCachePolicy>& cacheUTIFromMIMEType() WTF_REQUIRES_LOCK(cacheUTIFromMIMETypeLock)
{
    static NeverDestroyed<TinyLRUCache<String, RetainPtr<NSString>, 16, UTIFromMIMETypeCachePolicy>> cache;
    return cache;
}

String UTIFromMIMEType(const String& mimeType)
{
    Locker locker { cacheUTIFromMIMETypeLock };
    return cacheUTIFromMIMEType().get(mimeType).get();
}

bool isDeclaredUTI(const String& UTI)
{
    RetainPtr type = [UTType typeWithIdentifier:UTI];
    return type.get().isDeclared;
}

void setImageSourceAllowableTypes(const Vector<String>& supportedImageTypes)
{
#if HAVE(CGIMAGESOURCE_WITH_SET_ALLOWABLE_TYPES)
    auto allowableTypes = createNSArray(supportedImageTypes);
    CGImageSourceSetAllowableTypes((__bridge CFArrayRef)allowableTypes.get());
#else
    UNUSED_PARAM(supportedImageTypes);
#endif
}

} // namespace WebCore
