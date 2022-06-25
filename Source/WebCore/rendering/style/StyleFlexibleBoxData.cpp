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
    : flexGrow(RenderStyle::initialFlexGrow())
    , flexShrink(RenderStyle::initialFlexShrink())
    , flexBasis(RenderStyle::initialFlexBasis())
    , flexDirection(static_cast<unsigned>(RenderStyle::initialFlexDirection()))
    , flexWrap(static_cast<unsigned>(RenderStyle::initialFlexWrap()))
{
}

inline StyleFlexibleBoxData::StyleFlexibleBoxData(const StyleFlexibleBoxData& other)
    : RefCounted<StyleFlexibleBoxData>()
    , flexGrow(other.flexGrow)
    , flexShrink(other.flexShrink)
    , flexBasis(other.flexBasis)
    , flexDirection(other.flexDirection)
    , flexWrap(other.flexWrap)
{
}

Ref<StyleFlexibleBoxData> StyleFlexibleBoxData::copy() const
{
    return adoptRef(*new StyleFlexibleBoxData(*this));
}

bool StyleFlexibleBoxData::operator==(const StyleFlexibleBoxData& other) const
{
    return flexGrow == other.flexGrow && flexShrink == other.flexShrink && flexBasis == other.flexBasis
        && flexDirection == other.flexDirection && flexWrap == other.flexWrap;
}

}
