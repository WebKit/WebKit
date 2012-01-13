/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
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
 *  THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 *  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "StyleFlexibleBoxData.h"

#include "RenderStyle.h"

namespace WebCore {

StyleFlexibleBoxData::StyleFlexibleBoxData()
    : m_widthPositiveFlex(RenderStyle::initialFlexboxWidthPositiveFlex())
    , m_widthNegativeFlex(RenderStyle::initialFlexboxWidthNegativeFlex())
    , m_heightPositiveFlex(RenderStyle::initialFlexboxHeightPositiveFlex())
    , m_heightNegativeFlex(RenderStyle::initialFlexboxHeightNegativeFlex())
    , m_flexOrder(RenderStyle::initialFlexOrder())
    , m_flexPack(RenderStyle::initialFlexPack())
    , m_flexAlign(RenderStyle::initialFlexAlign())
    , m_flexItemAlign(RenderStyle::initialFlexItemAlign())
    , m_flexDirection(RenderStyle::initialFlexDirection())
    , m_flexWrap(RenderStyle::initialFlexWrap())
{
}

StyleFlexibleBoxData::StyleFlexibleBoxData(const StyleFlexibleBoxData& o)
    : RefCounted<StyleFlexibleBoxData>()
    , m_widthPositiveFlex(o.m_widthPositiveFlex)
    , m_widthNegativeFlex(o.m_widthNegativeFlex)
    , m_heightPositiveFlex(o.m_heightPositiveFlex)
    , m_heightNegativeFlex(o.m_heightNegativeFlex)
    , m_flexOrder(o.m_flexOrder)
    , m_flexPack(o.m_flexPack)
    , m_flexAlign(o.m_flexAlign)
    , m_flexItemAlign(o.m_flexItemAlign)
    , m_flexDirection(o.m_flexDirection)
    , m_flexWrap(o.m_flexWrap)
{
}

bool StyleFlexibleBoxData::operator==(const StyleFlexibleBoxData& o) const
{
    return m_widthPositiveFlex == o.m_widthPositiveFlex && m_widthNegativeFlex == o.m_widthNegativeFlex
        && m_heightPositiveFlex == o.m_heightPositiveFlex && m_heightNegativeFlex == o.m_heightNegativeFlex
        && m_flexOrder == o.m_flexOrder && m_flexPack == o.m_flexPack && m_flexAlign == o.m_flexAlign
        && m_flexItemAlign == o.m_flexItemAlign && m_flexDirection == o.m_flexDirection && m_flexWrap == o.m_flexWrap;
}

}
