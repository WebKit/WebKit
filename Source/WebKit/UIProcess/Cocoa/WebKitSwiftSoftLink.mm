/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/FileSystem.h>
#include <wtf/SoftLinking.h>

namespace WebKit {

extern void* WebKitSwiftLibrary(bool isOptional = false);
void* WebKitSwiftLibrary(bool isOptional)
{
    static void* library;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        // Start by searching for the library in DYLD_LIBRARY_PATH:
        if ((library = dlopen("libWebKitSwift.dylib", RTLD_NOW)))
            return;

        // Then search in the Frameworks/ directory of the currently loaded version of WebKit.framework:
        Dl_info info { };
        if (dladdr((const void*)&WebKitSwiftLibrary, &info) && strlen(info.dli_fname)) {
            auto dliPath = String::fromUTF8(info.dli_fname);
            if (dliPath.isNull())
                return;
            auto webkitFrameworkDirectory = WTF::FileSystemImpl::parentPath(dliPath);
            auto dylibPath = WTF::FileSystemImpl::pathByAppendingComponent(webkitFrameworkDirectory, "Frameworks/libWebKitSwift.dylib"_s);
            if ((library = dlopen(dylibPath.utf8().data(), RTLD_NOW)))
                return;
        }

        if (!isOptional)
            RELEASE_ASSERT_WITH_MESSAGE(library, "%s", dlerror());
    });
    return library;
}

}

SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL(WebKit, WebKitSwift, WKGroupSessionObserver)
SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL(WebKit, WebKitSwift, WKSLinearMediaContentMetadata)
SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL(WebKit, WebKitSwift, WKSLinearMediaPlayer)
SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL(WebKit, WebKitSwift, WKSLinearMediaTimeRange)
SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL(WebKit, WebKitSwift, WKSLinearMediaTrack)
SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL(WebKit, WebKitSwift, WKSPreviewWindowController)
SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL(WebKit, WebKitSwift, WKSRKEntity)
SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL(WebKit, WebKitSwift, WKSTextAnimationManager)
SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL(WebKit, WebKitSwift, WKIntelligenceTextEffectCoordinator)
SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL(WebKit, WebKitSwift, WKTextExtractionContainerItem)
SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL(WebKit, WebKitSwift, WKTextExtractionEditable)
SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL(WebKit, WebKitSwift, WKTextExtractionLink)
SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL(WebKit, WebKitSwift, WKTextExtractionTextItem)
SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL(WebKit, WebKitSwift, WKTextExtractionScrollableItem)
SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL(WebKit, WebKitSwift, WKTextExtractionImageItem)
SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL(WebKit, WebKitSwift, WKSLinearMediaSpatialVideoMetadata)
