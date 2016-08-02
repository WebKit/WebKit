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
#include "MathMLTextElement.h"

namespace WebCore {

class MathMLOperatorElement final : public MathMLTextElement {
public:
    static Ref<MathMLOperatorElement> create(const QualifiedName& tagName, Document&);
    static UChar parseOperatorText(const String&);
    UChar operatorText();
    void setOperatorFormDirty() { m_dictionaryProperty = Nullopt; }
    MathMLOperatorDictionary::Form form() { return dictionaryProperty().form; }
    unsigned short flags();
    Length defaultLeadingSpace();
    Length defaultTrailingSpace();

private:
    MathMLOperatorElement(const QualifiedName& tagName, Document&);
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;
    void childrenChanged(const ChildChange&) final;
    void parseAttribute(const QualifiedName&, const AtomicString&) final;

    Optional<UChar> m_operatorText;

    struct DictionaryProperty {
        MathMLOperatorDictionary::Form form;
        // Default leading and trailing spaces are "thickmathspace".
        unsigned short leadingSpaceInMathUnit { 5 };
        unsigned short trailingSpaceInMathUnit { 5 };
        // Default operator properties are all set to "false".
        unsigned short flags { 0 };
    };
    Optional<DictionaryProperty> m_dictionaryProperty;
    DictionaryProperty computeDictionaryProperty();
    const DictionaryProperty& dictionaryProperty();
};

}

#endif // ENABLE(MATHML)
