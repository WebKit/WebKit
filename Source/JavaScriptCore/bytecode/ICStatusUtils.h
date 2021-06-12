/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#pragma once

#include "BytecodeIndex.h"
#include "CacheableIdentifier.h"
#include "ExitFlag.h"

namespace JSC {

class CodeBlock;
class StructureSet;

template<typename VariantVectorType, typename VariantType>
bool appendICStatusVariant(VariantVectorType& variants, const VariantType& variant)
{
    // Attempt to merge this variant with an already existing variant.
    for (unsigned i = 0; i < variants.size(); ++i) {
        VariantType& mergedVariant = variants[i];
        if (mergedVariant.attemptToMerge(variant)) {
            for (unsigned j = 0; j < variants.size(); ++j) {
                if (i == j)
                    continue;
                if (variants[j].overlaps(mergedVariant))
                    return false;
            }
            return true;
        }
    }
    
    // Make sure there is no overlap. We should have pruned out opportunities for
    // overlap but it's possible that an inline cache got into a weird state. We are
    // defensive and bail if we detect crazy.
    for (unsigned i = 0; i < variants.size(); ++i) {
        if (variants[i].overlaps(variant))
            return false;
    }
    
    variants.append(variant);
    return true;
}

template<typename VariantVectorType>
void filterICStatusVariants(VariantVectorType& variants, const StructureSet& set)
{
    variants.removeAllMatching(
        [&] (auto& variant) -> bool {
            variant.structureSet().filter(set);
            return variant.structureSet().isEmpty();
        });
}

template<typename VariantVectorType>
CacheableIdentifier singleIdentifierForICStatus(VariantVectorType& variants)
{
    if (variants.isEmpty())
        return nullptr;

    CacheableIdentifier result = variants.first().identifier();
    if (!result)
        return nullptr;

    for (size_t i = 1; i < variants.size(); ++i) {
        CacheableIdentifier identifier = variants[i].identifier();
        if (!identifier || identifier != result)
            return nullptr;
    }

    return result;
}

ExitFlag hasBadCacheExitSite(CodeBlock* profiledBlock, BytecodeIndex);

} // namespace JSC

