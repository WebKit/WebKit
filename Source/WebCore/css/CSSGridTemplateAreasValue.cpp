/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013, 2014 Igalia S.L.
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

#include "config.h"
#include "CSSGridTemplateAreasValue.h"

#include <wtf/FixedVector.h>
#include <wtf/HashSet.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

CSSGridTemplateAreasValue::CSSGridTemplateAreasValue(CSS::GridTemplateAreas&& areas)
    : CSSValue(ClassType::GridTemplateAreas)
    , m_areas(WTFMove(areas))
{
    ASSERT(m_areas.map.rowCount);
    ASSERT(m_areas.map.columnCount);
}

CSSGridTemplateAreasValue::CSSGridTemplateAreasValue(const CSS::GridTemplateAreas& areas)
    : CSSGridTemplateAreasValue { CSS::GridTemplateAreas { areas } }
{
}

Ref<CSSGridTemplateAreasValue> CSSGridTemplateAreasValue::create(CSS::GridTemplateAreas&& areas)
{
    return adoptRef(*new CSSGridTemplateAreasValue(WTFMove(areas)));
}

Ref<CSSGridTemplateAreasValue> CSSGridTemplateAreasValue::create(const CSS::GridTemplateAreas& areas)
{
    return adoptRef(*new CSSGridTemplateAreasValue(areas));
}

String CSSGridTemplateAreasValue::stringForRow(size_t row) const
{
    FixedVector<String> columns(m_areas.map.columnCount);
    for (auto& it : m_areas.map.map) {
        auto& area = it.value;
        if (row >= area.rows.startLine() && row < area.rows.endLine()) {
            for (unsigned i = area.columns.startLine(); i < area.columns.endLine(); i++)
                columns[i] = it.key;
        }
    }
    StringBuilder builder;
    bool first = true;
    for (auto& name : columns) {
        if (!first)
            builder.append(' ');
        first = false;
        if (name.isNull())
            builder.append('.');
        else
            builder.append(name);
    }
    return builder.toString();
}

String CSSGridTemplateAreasValue::customCSSText(const CSS::SerializationContext& context) const
{
    return CSS::serializationForCSS(context, m_areas);
}

bool CSSGridTemplateAreasValue::equals(const CSSGridTemplateAreasValue& other) const
{
    return m_areas == other.m_areas;
}

} // namespace WebCore
