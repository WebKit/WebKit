/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSGridTemplateAreas.h"

#include "CSSGridTemplateAreasValue.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace CSS {

static void serializeNamedGridAreaMapPosition(StringBuilder& builder, const NamedGridAreaMap::Map& map, size_t row, size_t column)
{
    UncheckedKeyHashSet<String> candidates;
    for (auto& it : map) {
        auto& area = it.value;
        if (row >= area.rows.startLine() && row < area.rows.endLine())
            candidates.add(it.key);
    }
    for (auto& it : map) {
        auto& area = it.value;
        if (column >= area.columns.startLine() && column < area.columns.endLine() && candidates.contains(it.key)) {
            builder.append(it.key);
            return;
        }
    }
    builder.append("."_s);
}

void Serialize<NamedGridAreaMap>::operator()(StringBuilder& builder, const CSS::SerializationContext&, const NamedGridAreaMap& value)
{
    for (size_t row = 0; row < value.rowCount; ++row) {
        builder.append('"');
        for (size_t column = 0; column < value.columnCount; ++column) {
            serializeNamedGridAreaMapPosition(builder, value.map, row, column);
            if (column != value.columnCount - 1)
                builder.append(' ');
        }
        builder.append('"');
        if (row != value.rowCount - 1)
            builder.append(' ');
    }
}

Ref<CSSValue> CSSValueCreation<NamedGridAreaMap>::operator()(CSSValuePool&, const NamedGridAreaMap& value)
{
    return CSSGridTemplateAreasValue::create(value);
}

} // namespace CSS
} // namespace WebCore
