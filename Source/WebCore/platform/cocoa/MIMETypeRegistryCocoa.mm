/*
 * Copyright (C) 2006 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#import "config.h"
#import "MIMETypeRegistry.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <pal/spi/cocoa/CoreServicesSPI.h>
#import <pal/spi/cocoa/NSURLFileTypeMappingsSPI.h>
#import <pal/spi/cocoa/UniformTypeIdentifiersSPI.h>
#import <wtf/RobinHoodHashMap.h>
#import <wtf/RobinHoodHashSet.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/MakeString.h>

namespace WebCore {

static MemoryCompactLookupOnlyRobinHoodHashMap<RefPtr<StringImpl>, MemoryCompactLookupOnlyRobinHoodHashSet<RefPtr<StringImpl>>>& extensionsForMIMETypeMap()
{
    static NeverDestroyed extensionsForMIMETypeMap = [] {
        MemoryCompactLookupOnlyRobinHoodHashMap<RefPtr<StringImpl>, MemoryCompactLookupOnlyRobinHoodHashSet<RefPtr<StringImpl>>> map;

        auto addExtension = [&](StringImpl* type, StringImpl* extension) {
            map.add(type, MemoryCompactLookupOnlyRobinHoodHashSet<RefPtr<StringImpl>>()).iterator->value.add(extension);
        };

        auto addExtensions = [&](StringImpl* type, NSArray<NSString *> *extensions) {
            size_t pos = reverseFind(type, '/');

            ASSERT(pos != notFound);
            auto wildcardMIMEType = makeString(StringView(type).left(pos), "/*"_s);

            // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
            for (SUPPRESS_UNRETAINED_LOCAL NSString *extension in extensions) {
                // Add extension to wildcardMIMEType, for example add "png" to "image/*"
                addExtension(wildcardMIMEType, StringImpl::create(extension));
                // Add extension to its mimeType, for example add "png" to "image/png"
                addExtension(type, StringImpl::create(extension));
            }
        };

        [UTType _enumerateAllDeclaredTypesUsingBlock:^(UTType *utType, BOOL *) {
            RetainPtr<NSString> type = utType.preferredMIMEType;
            if (!type)
                return;
            RetainPtr extensions = dynamic_objc_cast<NSArray<NSString *>>(utType.tags[UTTagClassFilenameExtension]);
            if (!extensions || ![extensions count])
                return;
            addExtensions(StringImpl::create(type.get()), extensions.get());
        }];

        return map;
    }();

    return extensionsForMIMETypeMap;
}

// Specify MIME type <-> extension mappings for type identifiers recognized by the system that are missing MIME type values.
static const HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>, ASCIICaseInsensitiveHash>& additionalMimeTypesMap()
{
    static NeverDestroyed<HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>, ASCIICaseInsensitiveHash>> mimeTypesMap = [] {
        HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>, ASCIICaseInsensitiveHash> map;
        static constexpr auto additionalTypes = WTF::toArray<TypeExtensionPair>({
            // FIXME: Remove this list once rdar://112044000 (Many camera RAW image type identifiers are missing MIME types) is resolved.
            { "image/x-canon-cr2"_s, "cr2"_s },
            { "image/x-canon-cr3"_s, "cr3"_s },
            { "image/x-epson-erf"_s, "erf"_s },
            { "image/x-fuji-raf"_s, "raf"_s },
            { "image/x-hasselblad-3fr"_s, "3fr"_s },
            { "image/x-hasselblad-fff"_s, "fff"_s },
            { "image/x-leaf-mos"_s, "mos"_s },
            { "image/x-leica-rwl"_s, "rwl"_s },
            { "image/x-minolta-mrw"_s, "mrw"_s },
            { "image/x-nikon-nef"_s, "nef"_s },
            { "image/x-olympus-orf"_s, "orf"_s },
            { "image/x-panasonic-raw"_s, "raw"_s },
            { "image/x-panasonic-rw2"_s, "rw2"_s },
            { "image/x-pentax-pef"_s, "pef"_s },
            { "image/x-phaseone-iiq"_s, "iiq"_s },
            { "image/x-samsung-srw"_s, "srw"_s },
            { "image/x-sony-arw"_s, "arw"_s },
            { "image/x-sony-srf"_s, "srf"_s },
        });
        for (auto& [type, extension] : additionalTypes)
            map.add(extension, type);
        return map;
    }();
    return mimeTypesMap;
}

static const HashMap<RefPtr<StringImpl>, Vector<RefPtr<StringImpl>>, ASCIICaseInsensitiveHash>& additionalExtensionsMap()
{
    static NeverDestroyed<HashMap<RefPtr<StringImpl>, Vector<RefPtr<StringImpl>>, ASCIICaseInsensitiveHash>> extensionsMap = [] {
        HashMap<RefPtr<StringImpl>, Vector<RefPtr<StringImpl>>, ASCIICaseInsensitiveHash> map;
        for (auto& [extension, type] : additionalMimeTypesMap()) {
            map.ensure(type, [] {
                return Vector<RefPtr<StringImpl>>();
            }).iterator->value.append(extension);
        }
        return map;
    }();
    return extensionsMap;
}

static Vector<RefPtr<StringImpl>> extensionsForWildcardMIMEType(StringImpl* type)
{
    Vector<RefPtr<StringImpl>> extensions;

    auto iterator = extensionsForMIMETypeMap().find(type);
    if (iterator != extensionsForMIMETypeMap().end())
        extensions.appendRange(iterator->value.begin(), iterator->value.end());

    return extensions;
}

RefPtr<StringImpl> MIMETypeRegistry::mimeTypeForExtension(StringView extension)
{
    auto string = extension.createNSStringWithoutCopying();

    RetainPtr<NSString> mimeType = [[NSURLFileTypeMappings sharedMappings] MIMETypeForExtension:string.get()];
    if (mimeType.get().length)
        return StringImpl::create(mimeType.get());

    auto mapEntry = additionalMimeTypesMap().find<ASCIICaseInsensitiveStringViewHashTranslator>(extension);
    if (mapEntry != additionalMimeTypesMap().end())
        return mapEntry->value;

    return nullString();
}

Vector<RefPtr<StringImpl>> MIMETypeRegistry::extensionsForMIMEType(StringImpl* type)
{
    if (!type)
        return { };

    if (endsWith(type, '*'))
        return extensionsForWildcardMIMEType(type);

    RetainPtr<NSArray> extensions = [[NSURLFileTypeMappings sharedMappings] extensionsForMIMEType:createNSString(type).get()];
    if (extensions.get().count)
        return makeVector<RefPtr<StringImpl>>(extensions.get());

    auto mapEntry = additionalExtensionsMap().find(type);
    if (mapEntry != additionalExtensionsMap().end())
        return mapEntry->value;

    return { };
}

RefPtr<StringImpl> MIMETypeRegistry::preferredExtensionForMIMEType(StringImpl* type)
{
    if (!type)
        return nullString();

    // We accept some non-standard USD MIMETypes, so we can't rely on
    // the file type mappings.
    if (isUSDMIMEType(type))
        return "usdz"_si;

    RetainPtr preferredExtension = [[NSURLFileTypeMappings sharedMappings] preferredExtensionForMIMEType:createNSString(type).get()];
    if ([preferredExtension length])
        return StringImpl::create(preferredExtension.get());

    auto mapEntry = additionalExtensionsMap().find(type);
    if (mapEntry != additionalExtensionsMap().end())
        return mapEntry->value.first();

    return nullString();
}

bool MIMETypeRegistry::isApplicationPluginMIMEType(StringImpl* MIMEType)
{
#if ENABLE(PDF_PLUGIN)
    // FIXME: This should test if we're actually going to use PDFPlugin,
    // but we only know that in WebKit2 at the moment. This is not a problem
    // in practice because if we don't have PDFPlugin and we go to instantiate the
    // plugin, there won't exist an application plugin supporting these MIME types.
    if (isPDFMIMEType(MIMEType))
        return true;
#else
    UNUSED_PARAM(MIMEType);
#endif

    return false;
}

}
