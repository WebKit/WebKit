/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "Image.h"

#include "IntRect.h"
#include "MimeTypeRegistry.h"

namespace WebCore {

Image::Image(ImageAnimationObserver* observer)
    : m_animationObserver(observer)
{
}

Image::~Image()
{
}

bool Image::supportsType(const String& type)
{
    return MimeTypeRegistry::isSupportedImageResourceMIMEType(type); 
} 

bool Image::isNull() const
{
    return size().isEmpty();
}

bool Image::setData(bool allDataReceived)
{
    int length = m_data.size();
    if (!length)
        return true;

#ifdef kImageBytesCutoff
    // This is a hack to help with testing display of partially-loaded images.
    // To enable it, define kImageBytesCutoff to be a size smaller than that of the image files
    // being loaded. They'll never finish loading.
    if (length > kImageBytesCutoff) {
        length = kImageBytesCutoff;
        allDataReceived = false;
    }
#endif
    
#if PLATFORM(CG)
    // Avoid the extra copy of bytes by just handing the byte array directly to a CFDataRef.
    CFDataRef data = CFDataCreateWithBytesNoCopy(0, reinterpret_cast<const UInt8*>(m_data.data()), length, kCFAllocatorNull);
    bool result = setNativeData(data, allDataReceived);
    CFRelease(data);
#else
    bool result = setNativeData(&m_data, allDataReceived);
#endif

    return result;
}

IntRect Image::rect() const
{
    return IntRect(IntPoint(), size());
}

int Image::width() const
{
    return size().width();
}

int Image::height() const
{
    return size().height();
}

}
