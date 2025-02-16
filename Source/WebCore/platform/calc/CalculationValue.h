/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#pragma once

#include "CalculationRange.h"
#include "CalculationTree.h"
#include <wtf/Forward.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

namespace Calculation {
enum class Category : uint8_t;
}

class CalculationValue : public RefCounted<CalculationValue> {
    WTF_MAKE_FAST_COMPACT_ALLOCATED;
public:
    WEBCORE_EXPORT static Ref<CalculationValue> create(Calculation::Category, Calculation::Range, Calculation::Tree&&);
    WEBCORE_EXPORT ~CalculationValue();

    double evaluate(double percentResolutionLength) const;

    Calculation::Category category() const { return m_category; }
    Calculation::Range range() const { return m_range; }

    const Calculation::Tree& tree() const { return m_tree; }
    Calculation::Tree copyTree() const;
    Calculation::Child copyRoot() const;

    WEBCORE_EXPORT bool operator==(const CalculationValue&) const;

private:
    CalculationValue(Calculation::Category, Calculation::Range, Calculation::Tree&&);

    Calculation::Category m_category;
    Calculation::Range m_range;
    Calculation::Tree m_tree;
};

TextStream& operator<<(TextStream&, const CalculationValue&);

} // namespace WebCore

