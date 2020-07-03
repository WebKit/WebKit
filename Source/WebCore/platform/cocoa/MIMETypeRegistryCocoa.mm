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

#import <pal/spi/cocoa/CoreServicesSPI.h>
#import <pal/spi/cocoa/NSURLFileTypeMappingsSPI.h>
#import <wtf/cocoa/VectorCocoa.h>

namespace WebCore {

static HashMap<String, HashSet<String>>& extensionsForMIMETypeMap()
{
    static auto extensionsForMIMETypeMap = makeNeverDestroyed([] {
        HashMap<String, HashSet<String>> map;

        auto addExtension = [&](const String& type, const String& extension) {
            map.add(type, HashSet<String>()).iterator->value.add(extension);
        };

        auto addExtensions = [&](const String& type, NSArray<NSString *> *extensions) {
            size_t pos = type.reverseFind('/');

            ASSERT(pos != notFound);
            auto wildcardMIMEType = makeString(StringView(type).left(pos), "/*"_s);

            for (NSString *extension in extensions) {
                // Add extension to wildcardMIMEType, for example add "png" to "image/*"
                addExtension(wildcardMIMEType, extension);
                // Add extension to its mimeType, for example add "png" to "image/png"
                addExtension(type, extension);
            }
        };

        auto allUTIs = adoptCF(_UTCopyDeclaredTypeIdentifiers());

        for (NSString *uti in (__bridge NSArray<NSString *> *)allUTIs.get()) {
            auto type = adoptCF(UTTypeCopyPreferredTagWithClass((__bridge CFStringRef)uti, kUTTagClassMIMEType));
            if (!type)
                continue;
            auto extensions = adoptCF(UTTypeCopyAllTagsWithClass((__bridge CFStringRef)uti, kUTTagClassFilenameExtension));
            if (!extensions || !CFArrayGetCount(extensions.get()))
                continue;
            addExtensions(type.get(), (__bridge NSArray<NSString *> *)extensions.get());
        }

        return map;
    }());

    return extensionsForMIMETypeMap;
}

static Vector<String> extensionsForWildcardMIMEType(const String& type)
{
    Vector<String> extensions;

    auto iterator = extensionsForMIMETypeMap().find(type);
    if (iterator != extensionsForMIMETypeMap().end())
        extensions.appendRange(iterator->value.begin(), iterator->value.end());

    return extensions;
}

String MIMETypeRegistry::mimeTypeForExtension(const String& extension)
{
    return [[NSURLFileTypeMappings sharedMappings] MIMETypeForExtension:(NSString *)extension];
}

Vector<String> MIMETypeRegistry::extensionsForMIMEType(const String& type)
{
    if (type.endsWith('*'))
        return extensionsForWildcardMIMEType(type);
    return makeVector<String>([[NSURLFileTypeMappings sharedMappings] extensionsForMIMEType:type]);
}

String MIMETypeRegistry::preferredExtensionForMIMEType(const String& type)
{
    // System Previews accept some non-standard MIMETypes, so we can't rely on
    // the file type mappings.
    if (isSystemPreviewMIMEType(type))
        return "usdz"_s;

    return [[NSURLFileTypeMappings sharedMappings] preferredExtensionForMIMEType:(NSString *)type];
}

bool MIMETypeRegistry::isApplicationPluginMIMEType(const String& MIMEType)
{
#if ENABLE(PDFKIT_PLUGIN)
    // FIXME: This should test if we're actually going to use PDFPlugin,
    // but we only know that in WebKit2 at the moment. This is not a problem
    // in practice because if we don't have PDFPlugin and we go to instantiate the
    // plugin, there won't exist an application plugin supporting these MIME types.
    if (isPDFOrPostScriptMIMEType(MIMEType))
        return true;
#else
    UNUSED_PARAM(MIMEType);
#endif

    return false;
}

}
