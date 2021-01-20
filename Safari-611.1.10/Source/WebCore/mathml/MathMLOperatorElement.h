/*
 * Copyright (C) 2016 Igalia S.L. All rights reserved.
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

#pragma once

#if ENABLE(MATHML)

#include "MathMLOperatorDictionary.h"
#include "MathMLTokenElement.h"

namespace WebCore {

class MathMLOperatorElement final : public MathMLTokenElement {
    WTF_MAKE_ISO_ALLOCATED(MathMLOperatorElement);
public:
    static Ref<MathMLOperatorElement> create(const QualifiedName& tagName, Document&);
    struct OperatorChar {
        UChar32 character { 0 };
        bool isVertical { true };
    };
    static OperatorChar parseOperatorChar(const String&);
    const OperatorChar& operatorChar();
    void setOperatorFormDirty() { m_dictionaryProperty = WTF::nullopt; }
    MathMLOperatorDictionary::Form form() { return dictionaryProperty().form; }
    bool hasProperty(MathMLOperatorDictionary::Flag);
    Length defaultLeadingSpace();
    Length defaultTrailingSpace();
    const Length& leadingSpace();
    const Length& trailingSpace();
    const Length& minSize();
    const Length& maxSize();

private:
    MathMLOperatorElement(const QualifiedName& tagName, Document&);
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;
    void childrenChanged(const ChildChange&) final;
    void parseAttribute(const QualifiedName&, const AtomString&) final;

    Optional<OperatorChar> m_operatorChar;

    Optional<MathMLOperatorDictionary::Property> m_dictionaryProperty;
    MathMLOperatorDictionary::Property computeDictionaryProperty();
    const MathMLOperatorDictionary::Property& dictionaryProperty();

    struct OperatorProperties {
        unsigned short flags;
        unsigned short dirtyFlags { MathMLOperatorDictionary::allFlags };
    };
    OperatorProperties m_properties;
    void computeOperatorFlag(MathMLOperatorDictionary::Flag);

    Optional<Length> m_leadingSpace;
    Optional<Length> m_trailingSpace;
    Optional<Length> m_minSize;
    Optional<Length> m_maxSize;
};

}

#endif // ENABLE(MATHML)
