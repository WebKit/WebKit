/*
 * Copyright (C) 2017 Apple Inc.  All rights reserved.
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

#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>
#include <ImageIO/ImageIO.h>

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
#include "ArchiveFactory.h"
#endif

namespace WebCore {

const HashSet<String>& supportedDefaultImageSourceTypes()
{
    // CG at least supports the following standard image types:
    static NeverDestroyed<HashSet<String>> supportedDefaultImageSourceTypes = std::initializer_list<String> {
        "com.compuserve.gif",
        "com.microsoft.bmp",
        "com.microsoft.cur",
        "com.microsoft.ico",
        "public.jpeg",
        "public.jpeg-2000",
        "public.mpo-image",
        "public.png",
        "public.tiff",
    };

#ifndef NDEBUG
    // Make sure that CG supports them.
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        RetainPtr<CFArrayRef> systemImageSourceTypes = adoptCF(CGImageSourceCopyTypeIdentifiers());
        CFIndex count = CFArrayGetCount(systemImageSourceTypes.get());
        for (auto& imageSourceType : supportedDefaultImageSourceTypes.get()) {
            RetainPtr<CFStringRef> string = imageSourceType.createCFString();
            ASSERT(CFArrayContainsValue(systemImageSourceTypes.get(), CFRangeMake(0, count), string.get()));
        }
    });
#endif

    return supportedDefaultImageSourceTypes;
}

bool isSupportImageSourceType(const String& imageSourceType)
{
    return !imageSourceType.isEmpty() && supportedDefaultImageSourceTypes().contains(imageSourceType);
}

}

#endif
