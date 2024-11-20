/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSMathValue.h"

#include "CSSPrimitiveValue.h"
#include "Length.h"

namespace WebCore {

std::optional<CSSCalc::Tree> CSSMathValue::toCalcTree() const
{
    auto node = toCalcTreeNode();
    if (!node)
        return { };

    auto type = CSSCalc::getType(*node);
    auto category = type.calculationCategory();
    if (!category)
        return { };

    return CSSCalc::Tree {
        .root = WTFMove(*node),
        .type = type,
        .category = *category,
        .stage = CSSCalc::Stage::Specified,
        .range = CSS::All
    };
}

RefPtr<CSSValue> CSSMathValue::toCSSValue() const
{
    auto tree = toCalcTree();
    if (!tree)
        return nullptr;

    return CSSPrimitiveValue::create(CSSCalcValue::create(WTFMove(*tree)));
}

} // namespace WebCore
