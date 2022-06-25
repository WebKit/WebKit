/*
* Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "WKCACFImage.h"

#include "Image.h"
#include "WKCACFImageInternal.h"
#include <CoreFoundation/CFRuntime.h>
#include <wtf/RefPtr.h>

using namespace WKQCA;

struct _WKCACFImage : public CFRuntimeBase {
    RefPtr<Image> image;
};

static WKCACFImageRef toImage(CFTypeRef image)
{
    return static_cast<WKCACFImageRef>(const_cast<void*>(image));
}

size_t WKCACFImageGetWidth(WKCACFImageRef image)
{
    return image->image->size().cx;
}

size_t WKCACFImageGetHeight(WKCACFImageRef image)
{
    return image->image->size().cy;
}

HANDLE WKCACFImageCopyFileMapping(WKCACFImageRef image, size_t* fileMappingSize)
{
    return image->image->copyFileMapping(*fileMappingSize);
}

WKCACFImageRef WKCACFImageCreateWithImage(RefPtr<Image>&& image)
{
    WKCACFImageRef wrapper = toImage(_CFRuntimeCreateInstance(0, WKCACFImageGetTypeID(), sizeof(_WKCACFImage) - sizeof(CFRuntimeBase), 0));
    wrapper->image = WTFMove(image);
    return wrapper;
}

static void WKCACFImageFinalize(CFTypeRef image)
{
    toImage(image)->image = nullptr;
}

static CFStringRef WKCACFImageCopyFormattingDescription(CFTypeRef image, CFDictionaryRef options)
{
    return CFStringCreateWithFormat(CFGetAllocator(image), options, CFSTR("<WKCACFImage 0x%x>"), image);
}

static CFStringRef WKCACFImageCopyDebugDescription(CFTypeRef image)
{
    return WKCACFImageCopyFormattingDescription(image, 0);
}

CFTypeID WKCACFImageGetTypeID()
{
    static const CFRuntimeClass runtimeClass = {
        0,
        "WKCACFImage",
        0,
        0,
        WKCACFImageFinalize,
        0,
        0,
        WKCACFImageCopyFormattingDescription,
        WKCACFImageCopyDebugDescription,
        0
    };

    static CFTypeID id = _CFRuntimeRegisterClass(&runtimeClass);
    return id;
}
