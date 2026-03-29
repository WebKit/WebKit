/*
 * Copyright (C) 2026 saku
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
#include "StyleCustomIdent.h"

#include "CSSPrimitiveValue.h"
#include "StyleValueTypes.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

void add(Hasher& hasher, const CustomIdent& value)
{
    add(hasher, value.value);
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const CustomIdent& value)
{
    return ts << value.value;
}

// MARK: - Conversion

auto CSSValueCreation<CustomIdent>::operator()(CSSValuePool&, const RenderStyle&, const CustomIdent& value) -> Ref<CSSValue>
{
    return CSSPrimitiveValue::createCustomIdent(CSS::CustomIdent { value.value });
}

} // namespace Style
} // namespace WebCore
