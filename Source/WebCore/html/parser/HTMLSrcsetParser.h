/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HTMLSrcsetParser_h
#define HTMLSrcsetParser_h

#include <wtf/text/StringView.h>

namespace WebCore {

const int UninitializedDescriptor = -1;
const float DefaultDensityValue = 1.0;

class DescriptorParsingResult {
public:
    DescriptorParsingResult()
        : m_density(UninitializedDescriptor)
        , m_resourceWidth(UninitializedDescriptor)
        , m_resourceHeight(UninitializedDescriptor)
    {
    }

    bool hasDensity() const { return m_density >= 0; }
    bool hasWidth() const { return m_resourceWidth >= 0; }
    bool hasHeight() const { return m_resourceHeight >= 0; }

    float density() const { ASSERT(hasDensity()); return m_density; }
    unsigned resourceWidth() const { ASSERT(hasWidth()); return m_resourceWidth; }
    unsigned resourceHeight() const { ASSERT(hasHeight()); return m_resourceHeight; }

    void setResourceWidth(int width) { ASSERT(width >= 0); m_resourceWidth = (unsigned)width; }
    void setResourceHeight(int height) { ASSERT(height >= 0); m_resourceHeight = (unsigned)height; }
    void setDensity(float densityToSet) { ASSERT(densityToSet >= 0); m_density = densityToSet; }

private:
    float m_density;
    int m_resourceWidth;
    int m_resourceHeight;
};

struct ImageCandidate {
    enum OriginAttribute {
        SrcsetOrigin,
        SrcOrigin
    };

    ImageCandidate()
        : density(DefaultDensityValue)
        , resourceWidth(UninitializedDescriptor)
        , originAttribute(SrcsetOrigin)
    {
    }

    ImageCandidate(const StringView& source, const DescriptorParsingResult& result, OriginAttribute originAttribute)
        : string(source)
        , density(result.hasDensity() ? result.density() : UninitializedDescriptor)
        , resourceWidth(result.hasWidth() ? result.resourceWidth() : UninitializedDescriptor)
        , originAttribute(originAttribute)
    {
    }

    bool srcOrigin() const
    {
        return (originAttribute == SrcOrigin);
    }

    StringView string;
    float density;
    int resourceWidth;
    OriginAttribute originAttribute;
};

ImageCandidate bestFitSourceForImageAttributes(float deviceScaleFactor, const AtomicString& srcAttribute, const AtomicString& srcsetAttribute
#if ENABLE(PICTURE_SIZES)
    , unsigned sourceSize
#endif
    );

}

#endif
