/*
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
#include "ImageDecoder.h"

#include "IntSize.h"

namespace WebCore {

ImageDecoder::ImageDecoder()
  : m_decoder(0)
{}

ImageDecoder::~ImageDecoder()
{
    if (m_decoder)
        CFRelease(m_decoder);
}

const CFStringRef kCGImageSourceShouldPreferRGB32 = CFSTR("kCGImageSourceShouldPreferRGB32");

CFDictionaryRef imageSourceOptions()
{
    static CFDictionaryRef options;
    
    if (!options) {
        const void *keys[2] = { kCGImageSourceShouldCache, kCGImageSourceShouldPreferRGB32 };
        const void *values[2] = { kCFBooleanTrue, kCFBooleanTrue };
        options = CFDictionaryCreate(NULL, keys, values, 2, 
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }
    return options;
}

bool ImageDecoder::initialized() const
{
    return m_decoder;
}

void ImageDecoder::setData(NativeBytePtr data, bool allDataReceived)
{
    if (!m_decoder)
        m_decoder = CGImageSourceCreateIncremental(imageSourceOptions());
    CGImageSourceUpdateData(m_decoder, data, allDataReceived);
}

bool ImageDecoder::isSizeAvailable()
{
    bool result = false;
    CGImageSourceStatus imageSourceStatus = CGImageSourceGetStatus(m_decoder);

    // Ragnaros yells: TOO SOON! You have awakened me TOO SOON, Executus!
    if (imageSourceStatus >= kCGImageStatusIncomplete) {
        CFDictionaryRef image0Properties = CGImageSourceCopyPropertiesAtIndex(m_decoder, 0, imageSourceOptions());
        if (image0Properties) {
            CFNumberRef widthNumber = (CFNumberRef)CFDictionaryGetValue(image0Properties, kCGImagePropertyPixelWidth);
            CFNumberRef heightNumber = (CFNumberRef)CFDictionaryGetValue(image0Properties, kCGImagePropertyPixelHeight);
            result = widthNumber && heightNumber;
            CFRelease(image0Properties);
        }
    }
    
    return result;
}

IntSize ImageDecoder::size() const
{
    IntSize result;
    CFDictionaryRef properties = CGImageSourceCopyPropertiesAtIndex(m_decoder, 0, imageSourceOptions());
    if (properties) {
        int w = 0, h = 0;
        CFNumberRef num = (CFNumberRef)CFDictionaryGetValue(properties, kCGImagePropertyPixelWidth);
        if (num)
            CFNumberGetValue(num, kCFNumberIntType, &w);
        num = (CFNumberRef)CFDictionaryGetValue(properties, kCGImagePropertyPixelHeight);
        if (num)
            CFNumberGetValue(num, kCFNumberIntType, &h);
        result = IntSize(w, h);            
        CFRelease(properties);
    }
    return result;
}

int ImageDecoder::repetitionCount()
{
    int result = cAnimationLoopOnce; // No property means loop once.
        
    // A property with value 0 means loop forever.
    CFDictionaryRef properties = CGImageSourceCopyProperties(m_decoder, imageSourceOptions());
    if (properties) {
        CFDictionaryRef gifProperties = (CFDictionaryRef)CFDictionaryGetValue(properties, kCGImagePropertyGIFDictionary);
        if (gifProperties) {
            CFNumberRef num = (CFNumberRef)CFDictionaryGetValue(gifProperties, kCGImagePropertyGIFLoopCount);
            if (num)
                CFNumberGetValue(num, kCFNumberIntType, &result);
        } else
            result = cAnimationNone; // Turns out we're not a GIF after all, so we don't animate.
        
        CFRelease(properties);
    }
    
    return result;
}

size_t ImageDecoder::frameCount() const
{
    return m_decoder ? CGImageSourceGetCount(m_decoder) : 0;
}

CGImageRef ImageDecoder::createFrameAtIndex(size_t index)
{
    return CGImageSourceCreateImageAtIndex(m_decoder, index, imageSourceOptions());
}

float ImageDecoder::frameDurationAtIndex(size_t index)
{
    float duration = 0;
    CFDictionaryRef properties = CGImageSourceCopyPropertiesAtIndex(m_decoder, index, imageSourceOptions());
    if (properties) {
        CFDictionaryRef typeProperties = (CFDictionaryRef)CFDictionaryGetValue(properties, kCGImagePropertyGIFDictionary);
        if (typeProperties) {
            CFNumberRef num = (CFNumberRef)CFDictionaryGetValue(typeProperties, kCGImagePropertyGIFDelayTime);
            if (num) {
                CFNumberGetValue(num, kCFNumberFloatType, &duration);
                // Init the frame duration. Many annoying ads specify a 0 duration to make an image flash
                // as quickly as possible.  We follow Firefox's behavior and set the minimum duration to 
                // 100 ms.  See 4051389 for more details.
                if (duration < 0.1)
                    duration = 0.1;
            }
        }
        CFRelease(properties);
    }
    
    return duration;
}


}
