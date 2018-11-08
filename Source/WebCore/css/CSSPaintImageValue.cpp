/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "CSSPaintImageValue.h"

#if ENABLE(CSS_PAINTING_API)

#include "CSSVariableData.h"
#include "CustomPaintImage.h"
#include "PaintWorkletGlobalScope.h"
#include "RenderElement.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

String CSSPaintImageValue::customCSSText() const
{
    StringBuilder result;
    result.appendLiteral("paint(");
    result.append(m_name);
    // FIXME: print args.
    result.append(')');
    return result.toString();
}

RefPtr<Image> CSSPaintImageValue::image(RenderElement& renderElement, const FloatSize& size)
{
    if (size.isEmpty())
        return nullptr;
    auto* selectedGlobalScope = renderElement.document().paintWorkletGlobalScope();
    if (!selectedGlobalScope)
        return nullptr;
    auto locker = holdLock(selectedGlobalScope->paintDefinitionLock());
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
            else
                localRange.consume().serialize(builder);
        }
        if (!localRange.atEnd())
            localRange.consume(); // comma token
        arguments.append(builder.toString());
    }

    return CustomPaintImage::create(*registration, size, renderElement, arguments);
}

} // namespace WebCore

#endif
