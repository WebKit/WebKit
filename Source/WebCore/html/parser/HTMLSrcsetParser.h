/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HTMLSrcsetParser_h
#define HTMLSrcsetParser_h

#include <wtf/text/WTFString.h>

namespace WebCore {

class ImageWithScale {
public:
    ImageWithScale()
        : m_imageURLStart(0)
        , m_imageURLLength(0)
        , m_scaleFactor(1)
    {
    }

    ImageWithScale(unsigned start, unsigned length, float scaleFactor)
        : m_imageURLStart(start)
        , m_imageURLLength(length)
        , m_scaleFactor(scaleFactor)
    {
    }

    String imageURL(const String& srcAttribute, const String& srcsetAttribute) const
    {
        return m_imageURLLength ? srcsetAttribute.substringSharingImpl(m_imageURLStart, m_imageURLLength) : srcAttribute;
    }

    float scaleFactor() const
    {
        return m_scaleFactor;
    }

private:
    unsigned m_imageURLStart;
    unsigned m_imageURLLength;
    float m_scaleFactor;
};

// Space characters as defined by the HTML specification.
bool isHTMLSpace(UChar);
bool isHTMLLineBreak(UChar);
bool isNotHTMLSpace(UChar);
bool isHTMLSpaceButNotLineBreak(UChar character);

ImageWithScale bestFitSourceForImageAttributes(float deviceScaleFactor, const String& srcAttribute, const String& sourceSetAttribute);

}

#endif
