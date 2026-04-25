/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "CyclicModuleRecord.h"
#include "JSCell.h"
#include "JSPromise.h"
#include <wtf/RefPtr.h>

namespace JSC {

class ScriptFetcher;

// Table 45
// https://tc39.es/ecma262/#graphloadingstate-record
class ModuleGraphLoadingState final : public JSCell {
    friend class LLIntOffsetsExtractor;
public:
    using Base = JSCell;
    static constexpr unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    DECLARE_EXPORT_INFO;
    DECLARE_VISIT_CHILDREN;

    static constexpr DestructionMode needsDestruction = NeedsDestruction;
    static void destroy(JSCell*);

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.moduleGraphLoadingStateSpace<mode>();
    }

    inline static Structure* createStructure(VM&, JSGlobalObject*, JSValue);
    static ModuleGraphLoadingState* create(VM&, JSPromise*, RefPtr<ScriptFetcher>);

    JSPromise* promise() const;
    unsigned pendingModulesCount() const;
    bool isLoading() const;
    ScriptFetcher* scriptFetcher() const;

    void setPendingModulesCount(unsigned);
    void setIsLoading(bool);

    void appendVisited(VM&, CyclicModuleRecord*);
    bool containsVisited(CyclicModuleRecord*) const;
    void iterateVisited(auto&& function) const
    {
        for (const auto& barrier : m_visited)
            function(barrier.get());
    }

private:
    ModuleGraphLoadingState(VM&, Structure*, JSPromise*, RefPtr<ScriptFetcher>);

    void finishCreation(VM&);

    // [[PromiseCapability]]
    WriteBarrier<JSPromise> m_promise;
    // [[Visited]]
    Vector<WriteBarrier<CyclicModuleRecord>, 8> m_visited;
    // Contains the same contents as m_visited, so no write barriers needed.
    UncheckedKeyHashSet<CyclicModuleRecord*> m_visitedSet;
    // [[PendingModulesCount]]
    unsigned m_pendingModulesCount { 1 };
    // [[IsLoading]]
    bool m_isLoading { true };
    // [[HostDefined]]
    const RefPtr<ScriptFetcher> m_scriptFetcher;
};

} // namespace JSC
