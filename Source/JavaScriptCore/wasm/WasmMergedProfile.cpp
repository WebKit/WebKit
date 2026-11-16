/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "WasmMergedProfile.h"

#include "WasmModule.h"
#include "WasmModuleInformation.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(WEBASSEMBLY)

namespace JSC::Wasm {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MergedProfile);

MergedProfile::MergedProfile(const IPIntCallee& callee)
    : m_callSites(callee.numCallProfiles())
{
}

void MergedProfile::Candidates::markAsMegamorphic(uint32_t count)
{
    if (!m_isMegamorphic) {
        m_size = 0;
        m_callees.fill({ });
        m_isMegamorphic = true;
    }
    m_totalCount += count;
}

bool MergedProfile::Candidates::add(Callee* observedCallee, uint32_t observedCount)
{
    for (auto& [callee, count] : m_callees) {
        if (callee == observedCallee) {
            count += observedCount;
            return true;
        }
        if (!callee) {
            callee = observedCallee;
            count = observedCount;
            ++m_size;
            return true;
        }
    }
    return false;
}

void MergedProfile::Candidates::merge(IPIntCallee* target, const CallProfile& slot)
{
    EncodedJSValue boxedCallee = slot.boxedCallee();
    uint32_t speculativeTotalCount = slot.count();

    if (!boxedCallee) {
        // boxedCallee becomes nullptr when it is (1) a direct call or (2) an indirect call not recording anything yet.
        if (target) {
            // Direct call case.
            add(target, speculativeTotalCount);
        }
        m_totalCount += speculativeTotalCount;
        return;
    }

    if (CallProfile::isMegamorphic(boxedCallee)) {
        markAsMegamorphic(speculativeTotalCount);
        return;
    }

    // Let's not use slot.count() as we are concurrently reading polymorphic callee.
    // Make m_count consistent with all the polymorphic callee's counts.
    uint32_t addedCount = 0;
    if (auto* poly = CallProfile::polymorphic(boxedCallee)) {
        for (auto& profile : *poly) {
            if (SUPPRESS_UNCOUNTED_LOCAL auto* callee = CallProfile::monomorphic(profile.boxedCallee())) {
                if (add(callee, profile.count()))
                    addedCount += profile.count();
                else {
                    markAsMegamorphic(speculativeTotalCount);
                    return;
                }
            }
        }
    } else if (SUPPRESS_UNCOUNTED_LOCAL auto* callee = CallProfile::monomorphic(boxedCallee)) {
        if (add(callee, speculativeTotalCount))
            addedCount += speculativeTotalCount;
        else {
            markAsMegamorphic(speculativeTotalCount);
            return;
        }
    }
    m_totalCount += addedCount;
}

auto MergedProfile::Candidates::finalize() const -> Candidates
{
    Candidates result(*this);
    unsigned totalCount = 0;
    auto mutableSpan = std::span { result.m_callees }.first(result.m_size);
    std::ranges::sort(mutableSpan, [&](const auto& lhs, const auto& rhs) {
        return std::get<1>(lhs) > std::get<1>(rhs);
    });
    for (auto& [callee, count] : mutableSpan)
        totalCount += count;
    result.m_totalCount = totalCount;
    return result;
}

void MergedProfile::merge(const Module& module, const IPIntCallee& callee, BaselineData& data)
{
    m_totalCount += data.totalCount();
    m_merged = true;
    auto span = m_callSites.mutableSpan();
    RELEASE_ASSERT(data.size() == span.size());
    for (unsigned i = 0; i < data.size(); ++i) {
        IPIntCallee* target = nullptr;
        FunctionSpaceIndex index = callee.callTarget(i);
        if (index != FunctionSpaceIndex { }) {
            if (!module.moduleInformation().isImportedFunctionFromFunctionIndexSpace(index))
                target = module.ipintCallees().at(module.moduleInformation().toCodeIndex(index)).ptr();
        }
        span[i].merge(target, data.at(i));
    }
}

} // namespace JSC::Wasm

#endif
