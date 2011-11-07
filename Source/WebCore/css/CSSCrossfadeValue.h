/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef CSSCrossfadeValue_h
#define CSSCrossfadeValue_h

#include "CSSImageGeneratorValue.h"
#include "CSSImageValue.h"
#include "Image.h"
#include "ImageObserver.h"

namespace WebCore {

class RenderObject;

class CSSCrossfadeValue : public CSSImageGeneratorValue {
public:
    static PassRefPtr<CSSCrossfadeValue> create(PassRefPtr<CSSImageValue> fromImage, PassRefPtr<CSSImageValue> toImage) { return adoptRef(new CSSCrossfadeValue(fromImage, toImage)); }
    virtual ~CSSCrossfadeValue();

    virtual String cssText() const OVERRIDE;

    virtual PassRefPtr<Image> image(RenderObject*, const IntSize&) OVERRIDE;
    virtual bool isFixedSize() const OVERRIDE { return false; }
    virtual IntSize fixedSize(const RenderObject*) OVERRIDE;

    void setPercentage(PassRefPtr<CSSPrimitiveValue> percentage) { m_percentage = percentage; }

private:
    CSSCrossfadeValue(PassRefPtr<CSSImageValue> fromImage, PassRefPtr<CSSImageValue> toImage)
        : CSSImageGeneratorValue(CrossfadeClass)
        , m_fromImage(fromImage)
        , m_toImage(toImage)
    {
    }

    RefPtr<CSSImageValue> m_fromImage;
    RefPtr<CSSImageValue> m_toImage;
    RefPtr<CSSPrimitiveValue> m_percentage;
};

} // namespace WebCore

#endif // CSSCrossfadeValue_h
