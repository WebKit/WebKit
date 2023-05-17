/**
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "StyleNonInheritedData.h"

#include "StyleBoxData.h"
#include "StyleBackgroundData.h"
#include "StyleSurroundData.h"
#include "StyleMiscNonInheritedData.h"
#include "StyleRareNonInheritedData.h"

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleNonInheritedData);

StyleNonInheritedData::StyleNonInheritedData()
    : boxData(StyleBoxData::create())
    , backgroundData(StyleBackgroundData::create())
    , surroundData(StyleSurroundData::create())
    , miscData(StyleMiscNonInheritedData::create())
    , rareData(StyleRareNonInheritedData::create())
{
}

StyleNonInheritedData::StyleNonInheritedData(const StyleNonInheritedData& other)
    : RefCounted<StyleNonInheritedData>()
    , boxData(other.boxData)
    , backgroundData(other.backgroundData)
    , surroundData(other.surroundData)
    , miscData(other.miscData)
    , rareData(other.rareData)
{
}

Ref<StyleNonInheritedData> StyleNonInheritedData::create()
{
    return adoptRef(*new StyleNonInheritedData);
}

Ref<StyleNonInheritedData> StyleNonInheritedData::copy() const
{
    return adoptRef(*new StyleNonInheritedData(*this));
}

bool StyleNonInheritedData::operator==(const StyleNonInheritedData& other) const
{
    return boxData == other.boxData
        && backgroundData == other.backgroundData
        && surroundData == other.surroundData
        && miscData == other.miscData
        && rareData == other.rareData;
}

} // namespace WebCore
