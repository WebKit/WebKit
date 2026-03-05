/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include "StyleComputedStyle+DifferenceLogging.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {
namespace Style {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(NonInheritedData);

NonInheritedData::NonInheritedData()
    : boxData(BoxData::create())
    , backgroundData(BackgroundData::create())
    , surroundData(SurroundData::create())
    , miscData(NonInheritedMiscData::create())
    , rareData(NonInheritedRareData::create())
{
}

NonInheritedData::NonInheritedData(const NonInheritedData& other)
    : RefCounted<NonInheritedData>()
    , boxData(other.boxData)
    , backgroundData(other.backgroundData)
    , surroundData(other.surroundData)
    , miscData(other.miscData)
    , rareData(other.rareData)
{
    ASSERT(other == *this, "NonInheritedData should be properly copied.");
}

Ref<NonInheritedData> NonInheritedData::create()
{
    return adoptRef(*new NonInheritedData);
}

Ref<NonInheritedData> NonInheritedData::copy() const
{
    return adoptRef(*new NonInheritedData(*this));
}

bool NonInheritedData::operator==(const NonInheritedData& other) const
{
    return boxData == other.boxData
        && backgroundData == other.backgroundData
        && surroundData == other.surroundData
        && miscData == other.miscData
        && rareData == other.rareData;
}

#if !LOG_DISABLED
void NonInheritedData::dumpDifferences(TextStream& ts, const NonInheritedData& other) const
{
    boxData->dumpDifferences(ts, *other.boxData);
    backgroundData->dumpDifferences(ts, *other.backgroundData);
    surroundData->dumpDifferences(ts, *other.surroundData);

    miscData->dumpDifferences(ts, *other.miscData);
    rareData->dumpDifferences(ts, *other.rareData);
}
#endif

} // namespace Style
} // namespace WebCore
