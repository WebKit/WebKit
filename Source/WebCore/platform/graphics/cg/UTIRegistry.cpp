/*
 * Copyright (C) 2017-2022 Apple Inc.  All rights reserved.
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
#include "UTIRegistry.h"

#if USE(CG)

#include "ImageSourceCG.h"
#include "MIMETypeRegistry.h"

#include <ImageIO/ImageIO.h>
#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RobinHoodHashSet.h>

namespace WebCore {

const MemoryCompactLookupOnlyRobinHoodHashSet<String>& defaultSupportedImageTypes()
{
    static NeverDestroyed defaultSupportedImageTypes = [] {
        static constexpr std::array defaultSupportedImageTypes = {
            "com.compuserve.gif"_s,
            "com.microsoft.bmp"_s,
            "com.microsoft.cur"_s,
            "com.microsoft.ico"_s,
            "public.jpeg"_s,
            "public.png"_s,
            "public.tiff"_s,
#if !PLATFORM(WIN)
            "public.jpeg-2000"_s,
            "public.mpo-image"_s,
#endif
#if HAVE(WEBP)
            "public.webp"_s,
            "com.google.webp"_s,
            "org.webmproject.webp"_s,
#endif
#if HAVE(AVIF) || USE(AVIF)
            "public.avif"_s,
            "public.avis"_s,
#endif
        };

        auto systemSupportedCFImageTypes = adoptCF(CGImageSourceCopyTypeIdentifiers());
        CFIndex count = CFArrayGetCount(systemSupportedCFImageTypes.get());

        HashSet<String> systemSupportedImageTypes;
        CFArrayApplyFunction(systemSupportedCFImageTypes.get(), CFRangeMake(0, count), [](const void *value, void *context) {
            String imageType = static_cast<CFStringRef>(value);
            static_cast<HashSet<String>*>(context)->add(imageType);
        }, &systemSupportedImageTypes);

        MemoryCompactLookupOnlyRobinHoodHashSet<String> filtered;
        for (auto& imageType : defaultSupportedImageTypes) {
            if (systemSupportedImageTypes.contains(imageType))
                filtered.add(imageType);
        }
        return filtered;
    }();

    return defaultSupportedImageTypes;
}

MemoryCompactRobinHoodHashSet<String>& additionalSupportedImageTypes()
{
    static NeverDestroyed<MemoryCompactRobinHoodHashSet<String>> additionalSupportedImageTypes;
    return additionalSupportedImageTypes;
}

void setAdditionalSupportedImageTypes(const Vector<String>& imageTypes)
{
    MIMETypeRegistry::additionalSupportedImageMIMETypes().clear();
    for (const auto& imageType : imageTypes) {
        additionalSupportedImageTypes().add(imageType);
        auto mimeType = MIMETypeForImageType(imageType);
        if (!mimeType.isEmpty())
            MIMETypeRegistry::additionalSupportedImageMIMETypes().add(mimeType);
    }
}

void setAdditionalSupportedImageTypesForTesting(const String& imageTypes)
{
    setAdditionalSupportedImageTypes(imageTypes.split(';'));
}

bool isSupportedImageType(const String& imageType)
{
    if (imageType.isEmpty())
        return false;
    return defaultSupportedImageTypes().contains(imageType) || additionalSupportedImageTypes().contains(imageType);
}

bool isGIFImageType(StringView imageType)
{
    return imageType == "com.compuserve.gif"_s;
}

}

#endif
