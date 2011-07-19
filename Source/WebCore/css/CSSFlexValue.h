/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#ifndef CSSFlexValue_h
#define CSSFlexValue_h

#include "CSSPrimitiveValue.h"
#include "CSSValue.h"

#if ENABLE(CSS3_FLEXBOX)

namespace WebCore {

class CSSFlexValue : public CSSValue {
public:
    static PassRefPtr<CSSFlexValue> create(float positiveFlex, float negativeFlex, PassRefPtr<CSSPrimitiveValue> preferredSize)
    {
        return adoptRef(new CSSFlexValue(positiveFlex, negativeFlex, preferredSize));
    }

    virtual ~CSSFlexValue();

    virtual String cssText() const;

    virtual bool isFlexValue() const { return true; }

    float positiveFlex() { return m_positiveFlex; }
    float negativeFlex() { return m_negativeFlex; }
    CSSPrimitiveValue* preferredSize() { return m_preferredSize.get(); }

private:
    CSSFlexValue(float positiveFlex, float negativeFlex, PassRefPtr<CSSPrimitiveValue> preferredSize)
        : m_positiveFlex(positiveFlex)
        , m_negativeFlex(negativeFlex)
        , m_preferredSize(preferredSize)
    {
    }

    float m_positiveFlex;
    float m_negativeFlex;
    RefPtr<CSSPrimitiveValue> m_preferredSize;
};

}

#endif // ENABLE(CSS3_FLEXBOX)

#endif
