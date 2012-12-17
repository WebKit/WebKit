/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
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

#if ENABLE(CSS_EXCLUSIONS)

#include "ExclusionShapeOutsideInfo.h"

#include "RenderBox.h"
#include <wtf/HashMap.h>

namespace WebCore {

typedef HashMap<const RenderBox*, OwnPtr<ExclusionShapeOutsideInfo> > ExclusionShapeOutsideInfoMap;

static ExclusionShapeOutsideInfoMap& exclusionShapeOutsideInfoMap()
{
    DEFINE_STATIC_LOCAL(ExclusionShapeOutsideInfoMap, staticExclusionShapeOutsideInfoMap, ());
    return staticExclusionShapeOutsideInfoMap;
}

ExclusionShapeOutsideInfo::ExclusionShapeOutsideInfo(RenderBox* box)
    : m_box(box)
    , m_logicalWidth(0)
    , m_logicalHeight(0)
{
}

ExclusionShapeOutsideInfo::~ExclusionShapeOutsideInfo()
{
}

ExclusionShapeOutsideInfo* ExclusionShapeOutsideInfo::ensureInfoForRenderBox(RenderBox* box)
{
    ExclusionShapeOutsideInfoMap& infoMap = exclusionShapeOutsideInfoMap();
    if (ExclusionShapeOutsideInfo* shapeInfo = infoMap.get(box))
        return shapeInfo;

    ExclusionShapeOutsideInfoMap::AddResult result = infoMap.add(box, create(box));
    return result.iterator->value.get();
}

ExclusionShapeOutsideInfo* ExclusionShapeOutsideInfo::infoForRenderBox(const RenderBox* box)
{
    ASSERT(box->style()->shapeOutside());
    return exclusionShapeOutsideInfoMap().get(box);
}

bool ExclusionShapeOutsideInfo::isInfoEnabledForRenderBox(const RenderBox* box)
{
    // FIXME: Enable shape outside for non-rectangular shapes! (bug 98664)
    ExclusionShapeValue* value = box->style()->shapeOutside();
    return value && (value->type() == ExclusionShapeValue::SHAPE) && (value->shape()->type() == BasicShape::BASIC_SHAPE_RECTANGLE);
}

void ExclusionShapeOutsideInfo::removeInfoForRenderBox(const RenderBox* box)
{
    exclusionShapeOutsideInfoMap().remove(box);
}

const ExclusionShape* ExclusionShapeOutsideInfo::computedShape() const
{
    if (ExclusionShape* shapeOutside = m_computedShape.get())
        return shapeOutside;

    ExclusionShapeValue* basicShapeValue = m_box->style()->shapeOutside();
    ASSERT(basicShapeValue);
    ASSERT(basicShapeValue->type() == ExclusionShapeValue::SHAPE);

    m_computedShape = ExclusionShape::createExclusionShape(basicShapeValue->shape(), m_logicalWidth, m_logicalHeight, m_box->style()->writingMode());
    return m_computedShape.get();
}

}
#endif
