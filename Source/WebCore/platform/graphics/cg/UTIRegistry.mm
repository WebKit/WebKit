/*
 * Copyright (C) 2017-2025 Apple Inc.  All rights reserved.
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
#import "UTIRegistry.h"

#if USE(CG)

#import "MIMETypeRegistry.h"
#import "UTIUtilities.h"
#import <ImageIO/ImageIO.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <wtf/HashSet.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>
#import <wtf/RobinHoodHashSet.h>
#import <wtf/text/MakeString.h>

#if PLATFORM(IOS_FAMILY)
#import <MobileCoreServices/MobileCoreServices.h>
#endif

#if ENABLE(LOCKDOWN_MODE_API)
#import <pal/cocoa/LockdownModeCocoa.h>
#endif

namespace WebCore {

template<std::size_t size>
static MemoryCompactLookupOnlyRobinHoodHashSet<RefPtr<StringImpl>> filterSupportedImageTypes(const std::array<ASCIILiteral, size>& imageTypes)
{
    auto systemSupportedCFImageTypes = adoptCF(CGImageSourceCopyTypeIdentifiers());
    CFIndex count = CFArrayGetCount(systemSupportedCFImageTypes.get());

    HashSet<RefPtr<StringImpl>> systemSupportedImageTypes;
    CFArrayApplyFunction(systemSupportedCFImageTypes.get(), CFRangeMake(0, count), [](const void *value, void *context) {
        RefPtr<StringImpl> imageType = StringImpl::create(static_cast<CFStringRef>(value));
        static_cast<HashSet<RefPtr<StringImpl>>*>(context)->add(imageType);
    }, &systemSupportedImageTypes);

    MemoryCompactLookupOnlyRobinHoodHashSet<RefPtr<StringImpl>> filtered;
    for (auto& imageType : imageTypes) {
        if (systemSupportedImageTypes.contains(imageType))
            filtered.add(imageType);
    }

    return filtered;
}

static const MemoryCompactLookupOnlyRobinHoodHashSet<RefPtr<StringImpl>>& defaultSupportedImageTypes()
{
    static NeverDestroyed defaultSupportedImageTypes = [] {
        // Changes to supported formats may require updates in Info.plist
        // accepted formats for macOS clients such as Safari or MiniBrowse.
        static constexpr std::array defaultSupportedImageTypes = {
            "com.compuserve.gif"_s,
            "com.microsoft.bmp"_s,
            "com.microsoft.cur"_s,
            "com.microsoft.ico"_s,
            "public.jpeg"_s,
            "public.png"_s,
            "public.tiff"_s,
            "public.mpo-image"_s,
            "public.webp"_s,
            "com.google.webp"_s,
            "org.webmproject.webp"_s,
#if HAVE(AVIF)
            "public.avif"_s,
            "public.avis"_s,
#endif
#if HAVE(JPEGXL)
            "public.jxl"_s,
            "public.jpegxl"_s,
            "public.jpeg-xl"_s,
#endif
#if HAVE(HEIC)
            "public.heic"_s,
            "public.heics"_s,
            "public.heif"_s,
#endif
        };

        return filterSupportedImageTypes(defaultSupportedImageTypes);
    }();

    return defaultSupportedImageTypes;
}

#if ENABLE(LOCKDOWN_MODE_API)
static const MemoryCompactLookupOnlyRobinHoodHashSet<RefPtr<StringImpl>>& lockdownSupportedImageTypes()
{
    static NeverDestroyed lockdownSupportedImageTypes = [] {
        // Keep this list in sync with process-entitlements.sh.
        static constexpr std::array lockdownSupportedImageTypes = {
            "org.webmproject.webp"_s,
            "public.jpeg"_s,
            "public.png"_s,
            "com.compuserve.gif"_s,
        };

        return filterSupportedImageTypes(lockdownSupportedImageTypes);
    }();

    return lockdownSupportedImageTypes;
}
#endif

const MemoryCompactLookupOnlyRobinHoodHashSet<RefPtr<StringImpl>>& supportedImageTypes()
{
#if ENABLE(LOCKDOWN_MODE_API)
    if (PAL::isLockdownModeEnabledForCurrentProcess())
        return lockdownSupportedImageTypes();
#endif
    return defaultSupportedImageTypes();
}

MemoryCompactRobinHoodHashSet<RefPtr<StringImpl>>& additionalSupportedImageTypes()
{
    static NeverDestroyed<MemoryCompactRobinHoodHashSet<RefPtr<StringImpl>>> additionalSupportedImageTypes;
    return additionalSupportedImageTypes;
}

void setAdditionalSupportedImageTypes(const Vector<RefPtr<StringImpl>>& imageTypes)
{
#if ENABLE(LOCKDOWN_MODE_API)
    if (PAL::isLockdownModeEnabledForCurrentProcess())
        return;
#endif
    MIMETypeRegistry::additionalSupportedImageMIMETypes().clear();
    for (const auto& imageType : imageTypes) {
        additionalSupportedImageTypes().add(imageType);
        MIMETypeRegistry::additionalSupportedImageMIMETypes().addAll(RequiredMIMETypesFromUTI(imageType));
    }
}

void setAdditionalSupportedImageTypesForTesting(StringImpl* imageTypes)
{
    setAdditionalSupportedImageTypes(split(imageTypes, ';'));
}

bool isSupportedImageType(StringImpl* imageType)
{
    if (isEmpty(imageType))
        return false;
    return supportedImageTypes().contains(imageType) || additionalSupportedImageTypes().contains(imageType);
}

bool isGIFImageType(StringView imageType)
{
    return imageType == "com.compuserve.gif"_s;
}

RefPtr<StringImpl> MIMETypeForImageType(StringImpl* uti)
{
    return MIMETypeFromUTI(uti);
}

RefPtr<StringImpl> preferredExtensionForImageType(StringImpl* uti)
{
    return StringImpl::create([[UTType typeWithIdentifier:createNSString(uti).get()] preferredFilenameExtension]);
}

static Vector<RefPtr<StringImpl>> allowableDefaultSupportedImageTypes()
{
    auto allowableDefaultSupportedImageTypes = copyToVector(defaultSupportedImageTypes());
#if HAVE(AVIF)
    // AVIF might be embedded in a HEIF container. So HEIF/HEIC decoding have
    // to be allowed to get AVIF decoded.
    allowableDefaultSupportedImageTypes.append("public.heif"_s);
    allowableDefaultSupportedImageTypes.append("public.heic"_s);
#endif
    // JPEG2000 is supported only for PDF. Allow it at the process
    // level but disallow it in WebCore.
    allowableDefaultSupportedImageTypes.append("public.jpeg-2000"_s);
    allowableDefaultSupportedImageTypes.append("public.radiance"_s);
    allowableDefaultSupportedImageTypes.append("com.ilm.openexr-image"_s);
    allowableDefaultSupportedImageTypes.append("com.truevision.tga-image"_s);
    allowableDefaultSupportedImageTypes.append("org.khronos.ktx"_s);
    return allowableDefaultSupportedImageTypes;
}

#if ENABLE(LOCKDOWN_MODE_API)
static Vector<RefPtr<StringImpl>> allowableLockdownSupportedImageTypes()
{
    return copyToVector(lockdownSupportedImageTypes());
}
#endif

static Vector<RefPtr<StringImpl>> allowableSupportedImageTypes()
{
#if ENABLE(LOCKDOWN_MODE_API)
    if (PAL::isLockdownModeEnabledForCurrentProcess())
        return allowableLockdownSupportedImageTypes();
#endif
    return allowableDefaultSupportedImageTypes();
}

static Vector<RefPtr<StringImpl>> allowableAdditionalSupportedImageTypes()
{
    return copyToVector(additionalSupportedImageTypes());
}

Vector<RefPtr<StringImpl>> allowableImageTypes()
{
    auto allowableImageTypes = allowableSupportedImageTypes();
    auto additionalImageTypes = allowableAdditionalSupportedImageTypes();
    allowableImageTypes.appendVector(additionalImageTypes);
    return allowableImageTypes;
}

} // namespace WebCore

#endif // USE(CG)
