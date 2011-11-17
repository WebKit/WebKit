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
#include "CSSPrimitiveValue.h"
#include "Image.h"
#include "ImageObserver.h"

namespace WebCore {

class CachedImage;
class RenderObject;
class Document;

class CSSCrossfadeValue : public CSSImageGeneratorValue {
public:
    static PassRefPtr<CSSCrossfadeValue> create(PassRefPtr<CSSValue> fromImage, PassRefPtr<CSSValue> toImage)
    {
        return adoptRef(new CSSCrossfadeValue(fromImage, toImage));
    }

    String customCssText() const;

    PassRefPtr<Image> image(RenderObject*, const IntSize&);
    bool isFixedSize() const { return true; }
    IntSize fixedSize(const RenderObject*);

    bool isPending() const;
    void loadSubimages(CachedResourceLoader*);

    void setPercentage(PassRefPtr<CSSPrimitiveValue> percentage) { m_percentage = percentage; }

private:
    CSSCrossfadeValue(PassRefPtr<CSSValue> fromImage, PassRefPtr<CSSValue> toImage)
        : CSSImageGeneratorValue(CrossfadeClass)
        , m_fromImage(fromImage)
        , m_toImage(toImage)
        , m_crossfadeObserver(this) { }

    class CrossfadeObserverProxy : public ImageObserver {
    public:
        CrossfadeObserverProxy(CSSCrossfadeValue* ownerValue) : m_ownerValue(ownerValue) { }
        virtual ~CrossfadeObserverProxy() { }
        virtual void changedInRect(const Image*, const IntRect& rect) OVERRIDE { m_ownerValue->crossfadeChanged(rect); };
        virtual bool shouldPauseAnimation(const Image*) OVERRIDE { return false; }
        virtual void didDraw(const Image*) OVERRIDE { }
        virtual void animationAdvanced(const Image*) OVERRIDE { }
        virtual void decodedSizeChanged(const Image*, int) OVERRIDE { }
    private:
        CSSCrossfadeValue* m_ownerValue;
    };

    void crossfadeChanged(const IntRect&);

    RefPtr<CSSValue> m_fromImage;
    RefPtr<CSSValue> m_toImage;
    RefPtr<CSSPrimitiveValue> m_percentage;

    RefPtr<Image> m_generatedImage;

    CrossfadeObserverProxy m_crossfadeObserver;
};

} // namespace WebCore

#endif // CSSCrossfadeValue_h
