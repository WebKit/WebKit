/*
 * Copyright (C) 2009 Alex Milowski (alex@milowski.com). All rights reserved.
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

#include "RenderMathMLFencedOperator.h"
#include "RenderMathMLRow.h"

namespace WebCore {

class MathMLRowElement;

class RenderMathMLFenced final : public RenderMathMLRow {
public:
    RenderMathMLFenced(MathMLRowElement&, RenderStyle&&);

private:
    bool isRenderMathMLFenced() const final { return true; }
    const char* renderName() const final { return "RenderMathMLFenced"; }
    void addChild(RenderObject* child, RenderObject* beforeChild) final;
    void updateFromElement() final;

    RenderPtr<RenderMathMLFencedOperator> createMathMLOperator(const String& operatorString, MathMLOperatorDictionary::Form, MathMLOperatorDictionary::Flag);
    void makeFences();

    String m_open;
    String m_close;
    RefPtr<StringImpl> m_separators;

    RenderMathMLFencedOperator* m_closeFenceRenderer;
};

}

#endif // ENABLE(MATHML)
