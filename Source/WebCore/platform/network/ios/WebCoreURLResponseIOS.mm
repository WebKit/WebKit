/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#import "QuickLook.h"
#import "UTIUtilities.h"
#import <MobileCoreServices/MobileCoreServices.h>

#import "QuickLookSoftLink.h"

namespace WebCore {

// <rdar://problem/46332893> Register .mjs files as whatever UTI indicates JavaScript
static CFDictionaryRef createExtensionToMIMETypeMap()
{
    CFStringRef keys[] = {
        CFSTR("mjs")
    };

    CFStringRef values[] = {
        CFSTR("text/javascript")
    };

    ASSERT(sizeof(keys) == sizeof(values));
    return CFDictionaryCreate(kCFAllocatorDefault, (const void**)&keys, (const void**)&values, sizeof(keys) / sizeof(CFStringRef), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

void adjustMIMETypeIfNecessary(CFURLResponseRef cfResponse, bool isMainResourceLoad)
{
    RetainPtr<CFStringRef> mimeType = CFURLResponseGetMIMEType(cfResponse);
    RetainPtr<CFStringRef> updatedMIMEType = mimeType;
    if (!updatedMIMEType)
        updatedMIMEType = defaultMIMEType().createCFString();

    // <rdar://problem/46332893> Register .mjs files as whatever UTI indicates JavaScript
    if (!mimeType) {
        auto url = CFURLResponseGetURL(cfResponse);
        if ([(__bridge NSURL *)url isFileURL]) {
            RetainPtr<CFStringRef> extension = adoptCF(CFURLCopyPathExtension(url));
            if (extension) {
                static CFDictionaryRef extensionMap = createExtensionToMIMETypeMap();
                CFMutableStringRef mutableExtension = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, extension.get());
                CFStringLowercase(mutableExtension, NULL);
                extension = adoptCF(mutableExtension);
                if (auto newMIMEType = (CFStringRef)CFDictionaryGetValue(extensionMap, extension.get()))
                    updatedMIMEType = newMIMEType;
            }
        }
    }

#if USE(QUICK_LOOK)
    // We must ensure that the MIME type is correct, so that QuickLook's web plugin is called when needed.
    // We filter the basic MIME types so that we don't do unnecessary work in standard browsing situations.
    if (isMainResourceLoad && shouldUseQuickLookForMIMEType((NSString *)updatedMIMEType.get())) {
        RetainPtr<CFStringRef> suggestedFilename = adoptCF(CFURLResponseCopySuggestedFilename(cfResponse));
        RetainPtr<CFStringRef> quickLookMIMEType = adoptCF((CFStringRef)QLTypeCopyBestMimeTypeForFileNameAndMimeType((NSString *)suggestedFilename.get(), (NSString *)mimeType.get()));
        if (!quickLookMIMEType) {
            auto url = CFURLResponseGetURL(cfResponse);
            if ([(NSURL *)url isFileURL]) {
                RetainPtr<CFStringRef> extension = adoptCF(CFURLCopyPathExtension(url));
                if (extension) {
                    RetainPtr<CFStringRef> uti = adoptCF(UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, extension.get(), nullptr));
                    String MIMEType = MIMETypeFromUTITree(uti.get());
                    if (!MIMEType.isEmpty())
                        quickLookMIMEType = MIMEType.createCFString();
                }
            }
        }

        if (quickLookMIMEType)
            updatedMIMEType = quickLookMIMEType;
    }
#else
    UNUSED_PARAM(isMainResourceLoad);
#endif // USE(QUICK_LOOK)
    if (!mimeType || CFStringCompare(mimeType.get(), updatedMIMEType.get(), kCFCompareCaseInsensitive) != kCFCompareEqualTo)
        CFURLResponseSetMIMEType(cfResponse, updatedMIMEType.get());
}

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY)
