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

#include "AbstractModuleRecord.h"
#include "JSCell.h"
#include "JSModuleLoader.h"
#include <wtf/RefPtr.h>

namespace JSC {

class ModuleLoaderPayload;
class ModuleRegistryEntry;
class ScriptFetcher;

class ModuleLoadingContext final : public JSCell {
public:
    using Base = JSCell;
    static constexpr unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    static constexpr DestructionMode needsDestruction = NeedsDestruction;
    static void destroy(JSCell*);

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.moduleLoadingContextSpace<mode>();
    }

    DECLARE_INFO;
    DECLARE_VISIT_CHILDREN;

    inline static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    enum class Step : uint8_t {
        Main, // modulePromise settled -> loadRequestedModules
        Requested, // loadRequestedModules settled -> finishLoading + setEntryRecord
        Cached, // cached loadPromise settled -> finishLoading or evaluationError
    };

    static ModuleLoadingContext* create(VM&, Step, const JSModuleLoader::ModuleReferrer&, const AbstractModuleRecord::ModuleRequest&, ModuleLoaderPayload*, ModuleRegistryEntry*, RefPtr<ScriptFetcher>);
    static ModuleLoadingContext* create(VM&, const AbstractModuleRecord::ModuleRequest&, RefPtr<ScriptFetcher>, bool evaluate, bool dynamic, bool useImportMap);

    Step step() const { return m_step; }
    void setStep(Step s) { m_step = s; }

    JSModuleLoader::ModuleReferrer referrer() const;
    const AbstractModuleRecord::ModuleRequest& moduleRequest() const { return m_moduleRequest; }
    ModuleLoaderPayload* payload() const { return m_payload.get(); }
    ModuleRegistryEntry* entry() const { return m_entry.get(); }
    ScriptFetcher* scriptFetcher() const { return m_scriptFetcher.get(); }
    AbstractModuleRecord* module() const { return m_module.get(); }
    void module(VM& vm, AbstractModuleRecord* mod) { m_module.set(vm, this, mod); }

    bool evaluate() const { return m_evaluate; }
    bool dynamic() const { return m_dynamic; }
    bool useImportMap() const { return m_useImportMap; }

private:
    ModuleLoadingContext(VM&, Structure*, Step, const JSModuleLoader::ModuleReferrer&, AbstractModuleRecord::ModuleRequest&&, ModuleLoaderPayload*, ModuleRegistryEntry*, RefPtr<ScriptFetcher>);
    ModuleLoadingContext(VM&, Structure*, AbstractModuleRecord::ModuleRequest&&, RefPtr<ScriptFetcher>, bool evaluate, bool dynamic, bool useImportMap);

    Step m_step { Step::Main };
    AbstractModuleRecord::ModuleRequest m_moduleRequest;
    const RefPtr<ScriptFetcher> m_scriptFetcher;
    WriteBarrier<ModuleLoaderPayload> m_payload;
    WriteBarrier<ModuleRegistryEntry> m_entry;
    WriteBarrier<Unknown> m_referrer;
    WriteBarrier<AbstractModuleRecord> m_module;
    bool m_evaluate { false };
    bool m_dynamic { false };
    bool m_useImportMap { false };
};

} // namespace JSC
