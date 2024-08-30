/*
 * Copyright (C) 2022 Apple Inc.  All rights reserved.
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
#include "FilterResults.h"

#include "ImageBuffer.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FilterResults);

FilterResults::FilterResults(std::unique_ptr<ImageBufferAllocator>&& allocator)
    : m_allocator(allocator ? WTFMove(allocator) : makeUnique<ImageBufferAllocator>())
{
}

FilterImage* FilterResults::effectResult(FilterEffect& effect) const
{
    return m_results.get(effect);
}

size_t FilterResults::memoryCost() const
{
    CheckedSize memoryCost;

    for (auto& result : m_results.values())
        memoryCost += result->memoryCost();

    return memoryCost;
}

bool FilterResults::canCacheResult(const FilterImage& result) const
{
    static constexpr size_t maxAllowedMemoryCost = 100 * MB;
    CheckedSize totalMemoryCost = memoryCost();

    totalMemoryCost += result.memoryCost();
    if (totalMemoryCost.hasOverflowed())
        return false;

    return totalMemoryCost <= maxAllowedMemoryCost;
}

void FilterResults::setEffectResult(FilterEffect& effect, const FilterImageVector& inputs, Ref<FilterImage>&& result)
{
    if (!canCacheResult(result))
        return;

    m_results.set({ effect }, WTFMove(result));

    for (auto& input : inputs)
        m_resultReferences.add(input, FilterEffectSet()).iterator->value.add(effect);
}

void FilterResults::clearEffectResult(FilterEffect& effect)
{
    auto iterator = m_results.find(effect);
    if (iterator == m_results.end())
        return;

    auto result = iterator->value;
    m_results.remove(iterator);

    for (auto& reference : m_resultReferences.get(result))
        clearEffectResult(reference);

    m_resultReferences.remove(result);
}

} // namespace WebCore
