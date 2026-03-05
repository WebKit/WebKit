/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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
#include "StyleAppleColorFilterData.h"

#include "StyleComputedStyle+DifferenceLogging.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {
namespace Style {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AppleColorFilterData);

AppleColorFilterData::AppleColorFilterData()
    : appleColorFilter(ComputedStyle::initialAppleColorFilter())
{
}

AppleColorFilterData::AppleColorFilterData(const AppleColorFilterData& other)
    : RefCounted<AppleColorFilterData>()
    , appleColorFilter(other.appleColorFilter)
{
}

Ref<AppleColorFilterData> AppleColorFilterData::copy() const
{
    return adoptRef(*new AppleColorFilterData(*this));
}

bool AppleColorFilterData::operator==(const AppleColorFilterData& other) const
{
    return appleColorFilter == other.appleColorFilter;
}

#if !LOG_DISABLED
void AppleColorFilterData::dumpDifferences(TextStream& ts, const AppleColorFilterData& other) const
{
    LOG_IF_DIFFERENT(appleColorFilter);
}
#endif // !LOG_DISABLED

} // namespace Style
} // namespace WebCore
