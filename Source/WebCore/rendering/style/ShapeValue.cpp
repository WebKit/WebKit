/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ShapeValue.h"

#include "AnimationUtilities.h"
#include "CachedImage.h"
#include "StylePrimitiveNumericTypes+Blending.h"

namespace WebCore {

bool ShapeValue::isImageValid() const
{
    auto image = this->protectedImage();
    if (!image)
        return false;
    if (image->hasCachedImage()) {
        auto* cachedImage = image->cachedImage();
        return cachedImage && cachedImage->hasImage();
    }
    return image->isGeneratedImage();
}

bool ShapeValue::operator==(const ShapeValue& other) const
{
    return std::visit(WTF::makeVisitor(
        []<typename T>(const T& a, const T& b) {
            return a == b;
        },
        [](const auto&, const auto&) {
            return false;
        }
    ), m_value, other.m_value);
}

bool ShapeValue::canBlend(const ShapeValue& other) const
{
    return std::visit(WTF::makeVisitor(
        [](const ShapeAndBox& a, const ShapeAndBox& b) {
            return Style::canBlend(a.shape, b.shape) && a.box == b.box;
        },
        [](const auto&, const auto&) {
            return false;
        }
    ), m_value, other.m_value);
}

Ref<ShapeValue> ShapeValue::blend(const ShapeValue& other, const BlendingContext& context) const
{
    return std::visit(WTF::makeVisitor(
        [&](const ShapeAndBox& a, const ShapeAndBox& b) -> Ref<ShapeValue> {
            return ShapeValue::create(Style::blend(a.shape, b.shape, context), a.box);
        },
        [](const auto&, const auto&) -> Ref<ShapeValue> {
            RELEASE_ASSERT_NOT_REACHED();
        }
    ), m_value, other.m_value);
}

CSSBoxType ShapeValue::effectiveCSSBox() const
{
    return WTF::switchOn(m_value,
        [](const ShapeAndBox& shape) {
            return shape.box == CSSBoxType::BoxMissing ? CSSBoxType::MarginBox : shape.box;
        },
        [](const Ref<StyleImage>&) {
            return CSSBoxType::ContentBox;
        },
        [](const CSSBoxType& box) {
            return box == CSSBoxType::BoxMissing ? CSSBoxType::MarginBox : box;
        }
    );
}

} // namespace WebCore
