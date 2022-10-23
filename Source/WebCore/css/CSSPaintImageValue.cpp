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
#include "StylePaintImage.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSPaintImageValue::CSSPaintImageValue(String&& name, Ref<CSSVariableData>&& arguments)
    : CSSValue { PaintImageClass }
    , m_name { WTFMove(name) }
    , m_arguments { WTFMove(arguments) }
{
}

CSSPaintImageValue::~CSSPaintImageValue() = default;

String CSSPaintImageValue::customCSSText() const
{
    // FIXME: This should include the arguments too.
    return makeString("paint(", m_name, ')');
}

RefPtr<StyleImage> CSSPaintImageValue::createStyleImage(Style::BuilderState&) const
{
    return StylePaintImage::create(m_name, m_arguments);
}

} // namespace WebCore

#endif
