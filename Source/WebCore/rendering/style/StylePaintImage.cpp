/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "StylePaintImage.h"

#if ENABLE(CSS_PAINTING_API)

#include "CSSPaintImageValue.h"
#include "CSSVariableData.h"
#include "CustomPaintImage.h"
#include "PaintWorkletGlobalScope.h"
#include "RenderElement.h"
#include <wtf/PointerComparison.h>

namespace WebCore {

StylePaintImage::StylePaintImage(String&& name, Ref<CSSVariableData>&& arguments)
    : StyleGeneratedImage { Type::PaintImage, StylePaintImage::isFixedSize }
    , m_name { WTFMove(name) }
    , m_arguments { WTFMove(arguments) }
{
}

StylePaintImage::~StylePaintImage() = default;

bool StylePaintImage::operator==(const StyleImage& other) const
{
    // FIXME: Should probably also compare arguments?
    return is<StylePaintImage>(other) && downcast<StylePaintImage>(other).m_name == m_name;
}

Ref<CSSValue> StylePaintImage::computedStyleValue(const RenderStyle&) const
{
    return CSSPaintImageValue::create(m_name, m_arguments);
}

bool StylePaintImage::isPending() const
{
    return false;
}

void StylePaintImage::load(CachedResourceLoader&, const ResourceLoaderOptions&)
{
}

RefPtr<Image> StylePaintImage::image(const RenderElement* renderer, const FloatSize& size) const
{
    if (!renderer)
        return &Image::nullImage();

    if (size.isEmpty())
        return nullptr;

    auto* selectedGlobalScope = renderer->document().paintWorkletGlobalScopeForName(m_name);
    if (!selectedGlobalScope)
        return nullptr;

    Locker locker { selectedGlobalScope->paintDefinitionLock() };
    auto* registration = selectedGlobalScope->paintDefinitionMap().get(m_name);

    if (!registration)
        return nullptr;

    // FIXME: Check if argument list matches syntax.
    Vector<String> arguments;
    CSSParserTokenRange localRange(m_arguments->tokenRange());

    while (!localRange.atEnd()) {
        StringBuilder builder;
        while (!localRange.atEnd() && localRange.peek() != CommaToken) {
            if (localRange.peek() == CommentToken)
                localRange.consume();
            else if (localRange.peek().getBlockType() == CSSParserToken::BlockStart) {
                localRange.peek().serialize(builder);
                builder.append(localRange.consumeBlock().serialize(), ')');
            } else
                localRange.consume().serialize(builder);
        }
        if (!localRange.atEnd())
            localRange.consume(); // comma token
        arguments.append(builder.toString());
    }

    return CustomPaintImage::create(*registration, size, *renderer, arguments);
}

bool StylePaintImage::knownToBeOpaque(const RenderElement&) const
{
    return false;
}

FloatSize StylePaintImage::fixedSize(const RenderElement&) const
{
    return { };
}

} // namespace WebCore

#endif // ENABLE(CSS_PAINTING_API)
