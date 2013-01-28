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
#include "ExclusionShapeInfo.h"

#if ENABLE(CSS_EXCLUSIONS)

#include "ExclusionShape.h"
#include "RenderBlock.h"
#include "RenderBox.h"
#include "RenderStyle.h"

namespace WebCore {
template <class RenderType, ExclusionShapeValue* (RenderStyle::*shapeGetter)() const>
const ExclusionShape* ExclusionShapeInfo<RenderType, shapeGetter>::computedShape() const
{
    if (ExclusionShape* exclusionShape = m_shape.get())
        return exclusionShape;

    ExclusionShapeValue* shapeValue = (m_renderer->style()->*shapeGetter)();
    BasicShape* shape = (shapeValue && shapeValue->type() == ExclusionShapeValue::SHAPE) ? shapeValue->shape() : 0;

    ASSERT(shape);

    m_shape = ExclusionShape::createExclusionShape(shape, m_logicalWidth, m_logicalHeight, m_renderer->style()->writingMode());
    ASSERT(m_shape);
    return m_shape.get();
}

template class ExclusionShapeInfo<RenderBlock, &RenderStyle::shapeInside>;
template class ExclusionShapeInfo<RenderBox, &RenderStyle::shapeOutside>;
}
#endif
