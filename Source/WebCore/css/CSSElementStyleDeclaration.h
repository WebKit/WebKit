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

#ifndef CSSElementStyleDeclaration_h
#define CSSElementStyleDeclaration_h

#include "CSSMutableStyleDeclaration.h"

namespace WebCore {

class StyledElement;

// Base class for CSSInlineStyleDeclaration and FontFaceStyleDeclaration (SVG).

class CSSElementStyleDeclaration : public CSSMutableStyleDeclaration {
public:
    StyledElement* element() const { return m_element; }
    void clearElement() { m_element = 0; }

    virtual CSSStyleSheet* styleSheet() const;

protected:
    CSSElementStyleDeclaration(StyledElement* element, bool isInline)
        : CSSMutableStyleDeclaration()
        , m_element(element)
    {
        m_isElementStyleDeclaration = true;
        m_isInlineStyleDeclaration = isInline;
    }

    virtual ~CSSElementStyleDeclaration() { }

private:
    StyledElement* m_element;
};

} // namespace WebCore

#endif // CSSElementStyleDeclaration_h
