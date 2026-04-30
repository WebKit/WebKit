/*
 * Copyright (C) 2015-2022, 2026 Apple Inc. All rights reserved.
 * Copyright (C) 2016 Yusuke Suzuki <utatane.tea@gmail.com>.
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
#include "ErrorType.h"
#include "JSObject.h"
#include "ModuleGraphLoadingState.h"
#include "ModuleLoaderPayload.h"
#include "ModuleMap.h"

namespace JSC {

class ErrorInstance;
class JSPromise;
class JSModuleNamespaceObject;
class JSModuleRecord;
class JSSourceCode;
class ModuleRegistryEntry;

class JSModuleLoader final : public JSCell {
public:
    using Base = JSCell;
    static constexpr unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    static constexpr DestructionMode needsDestruction = NeedsDestruction;
    static void destroy(JSCell*);

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.moduleLoaderSpace<mode>();
    }

    enum Status {
        Fetch = 1,
        Instantiate,
        Satisfy,
        Link,
        Ready,
    };

    static JSModuleLoader* create(JSGlobalObject* globalObject, VM& vm, Structure* structure)
    {
        JSModuleLoader* object = new (NotNull, allocateCell<JSModuleLoader>(vm)) JSModuleLoader(vm, structure);
        object->finishCreation(globalObject, vm);
        return object;
    }

    static JSModuleLoader* create(JSGlobalObject* globalObject, VM& vm)
    {
        return create(globalObject, vm, vm.moduleLoaderStructure.get());
    }

    DECLARE_INFO;

    inline static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    // APIs to control the module loader.
    void provideFetch(JSGlobalObject*, const Identifier& key, ScriptFetchParameters::Type, SourceCode&&);
    void provideFetch(JSGlobalObject*, const Identifier& key, ScriptFetchParameters::Type, JSSourceCode*);
    JSPromise* loadModule(JSGlobalObject*, const Identifier& moduleName, RefPtr<ScriptFetchParameters>, RefPtr<ScriptFetcher>, bool evaluate, bool dynamic, bool useImportMap);
    JSPromise* linkAndEvaluateModule(JSGlobalObject*, const Identifier& moduleKey, RefPtr<ScriptFetchParameters>, RefPtr<ScriptFetcher>);
    JSPromise* requestImportModule(JSGlobalObject*, const Identifier& moduleName, const Identifier& referrer, RefPtr<ScriptFetchParameters>, RefPtr<ScriptFetcher>);

    // Platform dependent hooked APIs.
    JSPromise* importModule(JSGlobalObject*, JSString* moduleName, JSValue parameters, const SourceOrigin& referrer);
    Identifier resolve(JSGlobalObject*, JSValue name, JSValue referrer, RefPtr<ScriptFetcher>, bool useImportMap);
    Identifier resolve(JSGlobalObject*, const Identifier& name, const Identifier& referrer, RefPtr<ScriptFetcher>, bool useImportMap);
    JSPromise* fetch(JSGlobalObject*, JSValue key, RefPtr<ScriptFetchParameters>, RefPtr<ScriptFetcher>);
    JSObject* createImportMetaProperties(JSGlobalObject*, JSValue key, JSModuleRecord*, RefPtr<ScriptFetcher>);

    // Additional platform dependent hooked APIs.
    JSValue evaluate(JSGlobalObject*, JSValue key, JSValue moduleRecord, RefPtr<ScriptFetcher>, JSValue sentValue, JSValue resumeMode);
    JSValue evaluateNonVirtual(JSGlobalObject*, JSValue key, JSValue moduleRecord, RefPtr<ScriptFetcher>, JSValue sentValue, JSValue resumeMode);

    // Utility functions.
    JSModuleNamespaceObject* getModuleNamespaceObject(JSGlobalObject*, JSValue moduleRecord);
    JSArray* dependencyKeysIfEvaluated(JSGlobalObject*, const String& key);

    DECLARE_VISIT_CHILDREN;

    static AbstractModuleRecord* getImportedModule(AbstractModuleRecord* referrer, const AbstractModuleRecord::ModuleRequest&);
    static AbstractModuleRecord* maybeGetImportedModule(AbstractModuleRecord* referrer, const Identifier& moduleKey);

    // Options correspond to Script Records, Cyclic Module Records and Realm Records, in that order.
    struct ModuleReferrer : Variant<ProgramExecutable*, CyclicModuleRecord*, JSGlobalObject*> {
        using Variant<ProgramExecutable*, CyclicModuleRecord*, JSGlobalObject*>::Variant;
        ProgramExecutable* getScript() const;
        CyclicModuleRecord* getModule() const;
        JSGlobalObject* getRealm() const;
        bool isScript() const;
        bool isModule() const;
        bool isRealm() const;
        JSValue toJSValue() const;
    };

    struct ModuleFailure {
        enum class Kind {
            Unknown,
            Instantiation,
            Evaluation,
        };

        ModuleFailure() = default;
        ModuleFailure(AbstractModuleRecord*, ScriptFetchParameters::Type, Kind);
        ModuleFailure(Identifier, ScriptFetchParameters::Type, Kind);

        bool isEvaluationError(const Identifier& expectedSpecifier, ScriptFetchParameters::Type expectedType) const;

        operator bool() const;

        AbstractModuleRecord* m_source { nullptr };
        Identifier m_key;
        ScriptFetchParameters::Type m_type { ScriptFetchParameters::Type::None };
        Kind m_kind { Kind::Unknown };
    };

    using ModuleRequest = AbstractModuleRecord::ModuleRequest;
    using ModuleCompletion = Variant<AbstractModuleRecord*, Exception*>;

    void innerModuleLoading(JSGlobalObject*, ModuleGraphLoadingState*, AbstractModuleRecord*);
    // payload is opaque to callers and is either a ModuleGraphLoadingState* (graph load) or a ModuleLoaderPayload* (top-level dynamic import).
    void finishLoadingImportedModule(JSGlobalObject*, const ModuleReferrer&, const ModuleRequest&, JSCell* payload, ModuleCompletion result, RefPtr<ScriptFetcher>);

    JSPromise* hostLoadImportedModule(JSGlobalObject*, const ModuleReferrer&, const ModuleRequest&, JSCell* payload, RefPtr<ScriptFetcher>, bool useImportMap);
    JSPromise* loadModule(JSGlobalObject*, const ModuleReferrer&, const ModuleRequest&, JSCell* payload, RefPtr<ScriptFetcher>, bool evaluate, bool useImportMap);
    void continueModuleLoading(JSGlobalObject*, ModuleGraphLoadingState*, ModuleCompletion result);
    void continueDynamicImport(JSGlobalObject*, JSPromise*, ModuleCompletion, RefPtr<ScriptFetcher>);
    JSPromise* loadRequestedModules(JSGlobalObject*, AbstractModuleRecord*, RefPtr<ScriptFetcher>);

    static JSPromise* makeModule(JSGlobalObject*, const Identifier& moduleKey, JSSourceCode*);

    static ErrorInstance* duplicateTypeError(JSGlobalObject*, ErrorInstance*);
    static ErrorInstance* duplicateError(JSGlobalObject*, ErrorInstance*);
    static ErrorInstance* maybeDuplicateFetchError(JSGlobalObject*, ErrorInstance*);
    static ModuleFailure getErrorInfo(JSGlobalObject*, ErrorInstance*);
    static bool isFetchError(JSGlobalObject*, ErrorInstance*);
    static bool attachErrorInfo(JSGlobalObject*, Exception*, AbstractModuleRecord* source, const Identifier& key, ScriptFetchParameters::Type, ModuleFailure::Kind);
    static bool attachErrorInfo(JSGlobalObject*, ThrowScope&, AbstractModuleRecord* source, const Identifier& key, ScriptFetchParameters::Type, ModuleFailure::Kind);
    static void attachErrorInfo(JSGlobalObject*, ErrorInstance*, AbstractModuleRecord* source, const Identifier& key, ScriptFetchParameters::Type, ModuleFailure::Kind);

    ModuleRegistryEntry* ensureRegistered(JSGlobalObject*, const Identifier& key, ScriptFetchParameters::Type);

private:
    JSModuleLoader(VM&, Structure*);
    void finishCreation(JSGlobalObject*, VM&);

    ModuleRegistryEntry* getRegisteredMayBeNull(const Identifier& key, ScriptFetchParameters::Type);

    void addResolutionFailure(VM&, const ResolutionMapKey&, JSValue error);

    // Corresponds to RealmRecord.[[LoadedModules]].
    ModuleMap<AbstractModuleRecord::LoadedModuleRequest> m_loadedModules;

    ModuleMap<WriteBarrier<ModuleRegistryEntry>> m_moduleMap;

    ResolutionMap<WriteBarrier<Unknown>> m_resolutionFailures;
};

// Validates the host-defined payload threaded through HostLoadImportedModule / FinishLoadingImportedModule.
// Spec's `payload ∈ { GraphLoadingState Record, PromiseCapability Record }` is encoded in JSC as
// either ModuleGraphLoadingState* (graph load) or ModuleLoaderPayload* (top-level dynamic import).
inline bool isModuleLoaderHostDefinedPayload(JSCell* cell)
{
    return cell->inherits<ModuleGraphLoadingState>() || cell->inherits<ModuleLoaderPayload>();
}

} // namespace JSC
