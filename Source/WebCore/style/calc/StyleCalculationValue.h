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

#include <WebCore/CSSPrimitiveNumericRange.h>
#include <WebCore/StyleCalculationTree.h>
#include <wtf/Forward.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

namespace CSS {
enum class Category : uint8_t;
}

namespace Style {

struct ZoomFactor;
struct ZoomNeeded;

namespace Calculation {

class Value : public RefCounted<Value> {
    WTF_DEPRECATED_MAKE_FAST_COMPACT_ALLOCATED(Value);
public:
    WEBCORE_EXPORT static Ref<Value> NODELETE create(CSS::Category, CSS::Range, Tree&&);
    WEBCORE_EXPORT ~Value();

    double evaluate(double percentResolutionLength, const ZoomFactor& usedZoom) const;
    double evaluate(double percentResolutionLength, const ZoomNeeded&) const;

    CSS::Category category() const { return m_category; }
    CSS::Range range() const { return m_range; }

    const Tree& tree() const { return m_tree; }
    Tree copyTree() const;
    Child copyRoot() const;

    WEBCORE_EXPORT bool operator==(const Value&) const;

private:
    Value(CSS::Category, CSS::Range, Tree&&);

    CSS::Category m_category;
    CSS::Range m_range;
    Tree m_tree;
};

TextStream& operator<<(TextStream&, const Value&);

} // namespace Calculation
} // namespace Style
} // namespace WebCore
