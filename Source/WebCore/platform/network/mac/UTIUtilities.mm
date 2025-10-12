/*
 * Copyright (C) 2012-2025 Apple Inc. All rights reserved.
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
#import <pal/spi/cg/ImageIOSPI.h>
#import <pal/spi/cocoa/UniformTypeIdentifiersSPI.h>
#import <wtf/HashSet.h>
#import <wtf/Lock.h>
#import <wtf/MainThread.h>
#import <wtf/SortedArrayMap.h>
#import <wtf/TinyLRUCache.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)
#import <MobileCoreServices/MobileCoreServices.h>
#endif

namespace WebCore {

String MIMETypeFromUTI(const String& uti)
{
    RetainPtr type = [UTType typeWithIdentifier:uti.createNSString().get()];
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


RetainPtr<NSString> mimeTypeFromUTITree(UTType *utType)
{
    if (utType.declared || utType.dynamic)
        return utType.preferredMIMEType;

    // If not, walk the ancestory of this UTI to find a valid MIME type:
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // Note: We have to silence deprecated declarations because some types this
    // method returns are deprecated.
    RetainPtr<NSOrderedSet<UTType *>> parentTypes = utType._parentTypes;
ALLOW_DEPRECATED_DECLARATIONS_END
    if (!parentTypes)
        return nullptr;

    for (UTType *parentType : parentTypes.get()) {
        if (auto&& type = mimeTypeFromUTITree(parentType))
            return WTFMove(type);
    }

    return nullptr;
}

RetainPtr<CFStringRef> mimeTypeFromUTITree(CFStringRef uti)
{
    // Check if this UTI has a MIME type.
    RetainPtr utType = [UTType typeWithIdentifier:bridge_cast(uti)];
    if (!utType)
        return nullptr;

    if (auto mimeType = mimeTypeFromUTITree(utType.get()))
        return bridge_cast(mimeType.get());

    return nullptr;
}

static NSString *UTIFromPotentiallyUnknownMIMEType(StringView mimeType)
{
    static constexpr std::pair<ComparableLettersLiteral, NSString *> typesArray[] = {
        { "model/usd"_s, @"com.pixar.universal-scene-description-mobile" },
        { "model/vnd.pixar.usd"_s, @"com.pixar.universal-scene-description-mobile" },
        { "model/vnd.reality"_s, @"com.apple.reality" },
        { "model/vnd.usdz+zip"_s, @"com.pixar.universal-scene-description-mobile" },
    };
    static constexpr SortedArrayMap typesMap { typesArray };
    return typesMap.get(mimeType, nil);
}

struct UTIFromMIMETypeCachePolicy : TinyLRUCachePolicy<String, RetainPtr<NSString>> {
public:
    static RetainPtr<NSString> createValueForKey(const String& mimeType)
    {
        if (RetainPtr type = UTIFromPotentiallyUnknownMIMEType(mimeType))
            return type;

        if (RetainPtr type = [UTType typeWithMIMEType:mimeType.createNSString().get()])
            return type.get().identifier;

        return @"";
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

bool isDeclaredUTI(const String& uti)
{
    RetainPtr type = [UTType typeWithIdentifier:uti.createNSString().get()];
    return type.get().isDeclared;
}

void setImageSourceAllowableTypes(const Vector<String>& supportedImageTypes)
{
    // A WebPage might be reinitialized. So restrict ImageIO to the default and
    // the additional supported image formats only once.
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [supportedImageTypes] {
        auto allowableTypes = createNSArray(supportedImageTypes);
        auto status = CGImageSourceSetAllowableTypes((__bridge CFArrayRef)allowableTypes.get());
        RELEASE_ASSERT_WITH_MESSAGE(supportedImageTypes.isEmpty() || status == noErr, "CGImageSourceSetAllowableTypes() returned error: %d.", status);
    });
}

} // namespace WebCore
