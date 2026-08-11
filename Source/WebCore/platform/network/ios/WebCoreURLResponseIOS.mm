/*
 * Copyright (C) 2009-2021 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
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
#import "WebCoreURLResponseIOS.h"

#if PLATFORM(IOS_FAMILY)

#import "FrameLoaderTypes.h"
#import "QuickLook.h"
#import <MobileCoreServices/MobileCoreServices.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

#import <pal/ios/QuickLookSoftLink.h>

namespace WebCore {

static inline bool shouldPreferTextPlainMIMEType(const String& mimeType, const String& proposedMIMEType)
{
    return ("text/plain"_s == mimeType) && ((proposedMIMEType == "text/xml"_s) || (proposedMIMEType == "application/xhtml+xml"_s) || (proposedMIMEType == "application/xml"_s) || (proposedMIMEType == "image/svg+xml"_s));
}

void adjustMIMETypeIfNecessary(CFURLResponseRef response, IsMainResourceLoad isMainResourceLoad, IsNoSniffSet isNoSniffSet)
{
    auto type = CFURLResponseGetMIMEType(response);
    if (!type) {
        // FIXME: <rdar://problem/46332893> is fixed, but for some reason, this special case is still needed; should resolve that issue and remove this.
        if (auto extension = filePathExtension(response)) {
            if (CFStringCompare(extension.get(), CFSTR("mjs"), kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
                CFURLResponseSetMIMEType(response, CFSTR("text/javascript"));
                return;
            }
        }
    }

#if !USE(QUICK_LOOK)
    UNUSED_PARAM(isMainResourceLoad);
    UNUSED_PARAM(isNoSniffSet);
#else
    // Ensure that the MIME type is correct so that QuickLook's web plug-in is called when needed.
    // The shouldUseQuickLookForMIMEType function filters out the common MIME types so we don't do unnecessary work in those cases.
    if (isMainResourceLoad == IsMainResourceLoad::Yes && isNoSniffSet == IsNoSniffSet::No && shouldUseQuickLookForMIMEType(bridge_cast(type))) {
        RetainPtr<NSString> updatedType;
        auto suggestedFilename = adoptCF(CFURLResponseCopySuggestedFilename(response));
        if (auto quickLookType = adoptNS(PAL::softLink_QuickLook_QLTypeCopyBestMimeTypeForFileNameAndMimeType(bridge_cast(suggestedFilename.get()), bridge_cast(type))))
            updatedType = quickLookType.get();
        else if (auto extension = filePathExtension(response))
            updatedType = preferredMIMETypeForFileExtensionFromUTType(bridge_cast(extension.get()));
        if (updatedType && !shouldPreferTextPlainMIMEType(type, updatedType.get()) && (!type || CFStringCompare(type, bridge_cast(updatedType.get()), kCFCompareCaseInsensitive) != kCFCompareEqualTo)) {
            CFURLResponseSetMIMEType(response, bridge_cast(updatedType.get()));
            return;
        }
    }
#endif // USE(QUICK_LOOK)
    if (!type)
        CFURLResponseSetMIMEType(response, CFSTR("application/octet-stream"));
}

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY)
