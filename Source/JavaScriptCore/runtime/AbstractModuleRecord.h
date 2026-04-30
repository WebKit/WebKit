/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/Identifier.h>
#include <JavaScriptCore/JSGenerator.h>
#include <JavaScriptCore/JSInternalFieldObjectImpl.h>
#include <JavaScriptCore/ModuleMap.h>
#include <JavaScriptCore/ScriptFetchParameters.h>
#include <JavaScriptCore/ScriptFetcher.h>
#include <wtf/ListHashSet.h>
#include <wtf/RefPtr.h>

namespace JSC {

class CyclicModuleRecord;
class JSModuleEnvironment;
class JSModuleNamespaceObject;
class JSMap;
class JSPromise;

// Based on the Source Text Module Record
// http://www.ecma-international.org/ecma-262/6.0/#sec-source-text-module-records
class AbstractModuleRecord : public JSInternalFieldObjectImpl<2> {
    friend class LLIntOffsetsExtractor;
public:
    using Base = JSInternalFieldObjectImpl<2>;

    static constexpr DestructionMode needsDestruction = NeedsDestruction;

    template<typename CellType, SubspaceAccess>
    static void subspaceFor(VM&)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    using Argument = JSGenerator::Argument;
    using State = JSGenerator::State;
    using ResumeMode = JSGenerator::ResumeMode;

    enum class Field : uint32_t {
        State,
        Frame,
    };

    static_assert(numberOfInternalFields == 2);
    static std::array<JSValue, numberOfInternalFields> initialValues()
    {
        return { {
            jsNumber(static_cast<int32_t>(State::Init)),
            jsUndefined(),
        } };
    }

    // https://tc39.github.io/ecma262/#sec-source-text-module-records
    struct ExportEntry {
        enum class Type {
            Local,
            Indirect,
            Namespace,
        };

        static ExportEntry NODELETE createLocal(const Identifier& exportName, const Identifier& localName);
        static ExportEntry NODELETE createIndirect(const Identifier& exportName, const Identifier& importName, const Identifier& moduleName);
        static ExportEntry NODELETE createNamespace(const Identifier& exportName, const Identifier& moduleName);

        Type type;
        Identifier exportName;
        Identifier moduleName;
        Identifier importName;
        Identifier localName;
    };

    enum class ImportEntryType { Single, Namespace };
    struct ImportEntry {
        ImportEntryType type;
        Identifier moduleRequest;
        Identifier importName;
        Identifier localName;
    };

    typedef WTF::ListHashSet<RefPtr<UniquedStringImpl>, IdentifierRepHash> OrderedIdentifierSet;
    typedef UncheckedKeyHashMap<RefPtr<UniquedStringImpl>, ImportEntry, IdentifierRepHash, HashTraits<RefPtr<UniquedStringImpl>>> ImportEntries;
    typedef UncheckedKeyHashMap<RefPtr<UniquedStringImpl>, ExportEntry, IdentifierRepHash, HashTraits<RefPtr<UniquedStringImpl>>> ExportEntries;

    struct ModuleRequest {
        Identifier m_specifier;
        RefPtr<ScriptFetchParameters> m_attributes;

        ScriptFetchParameters::Type type(ScriptFetchParameters::Type fallback = ScriptFetchParameters::Type::JavaScript) const;
        bool operator==(const ModuleRequest&) const;
    };

    struct LoadedModuleRequest : ModuleRequest {
        LoadedModuleRequest() = default;
        LoadedModuleRequest(VM&, ModuleRequest, AbstractModuleRecord* loadedModule, JSCell* owner);
        WriteBarrier<AbstractModuleRecord> m_module;
    };

    DECLARE_EXPORT_INFO;

    void appendRequestedModule(const Identifier&, RefPtr<ScriptFetchParameters>&&);
    void addStarExportEntry(const Identifier&);
    void addImportEntry(const ImportEntry&);
    void addExportEntry(const ExportEntry&);

    std::optional<ImportEntry> NODELETE tryGetImportEntry(UniquedStringImpl* localName);
    std::optional<ExportEntry> NODELETE tryGetExportEntry(UniquedStringImpl* exportName);

    class AsyncEvaluationOrder {
    public:
        AsyncEvaluationOrder() = default;
        AsyncEvaluationOrder(int64_t order);

        bool isDone() const { return m_order == Done; }
        bool isUnset() const { return m_order == Unset; }
        bool hasOrder() const { return m_order >= 0; }
        void setDone() { m_order = Done; }

        int64_t order() const;
        AsyncEvaluationOrder& order(int64_t);

        static AsyncEvaluationOrder done() { return { Done }; }

    private:
        static constexpr int64_t Unset = -2;
        static constexpr int64_t Done = -1;
        int64_t m_order { Unset };
    };

    const Identifier& moduleKey() const { return m_moduleKey; }
    ScriptFetchParameters::Type moduleType() const;
    const Vector<ModuleRequest>& requestedModules() const LIFETIME_BOUND { return m_requestedModules; }
    ModuleMap<LoadedModuleRequest>& loadedModules() LIFETIME_BOUND { return m_loadedModules; }
    const ModuleMap<LoadedModuleRequest>& loadedModules() const LIFETIME_BOUND { return m_loadedModules; }
    const ExportEntries& exportEntries() const LIFETIME_BOUND { return m_exportEntries; }
    const ImportEntries& importEntries() const LIFETIME_BOUND { return m_importEntries; }
    const OrderedIdentifierSet& starExportEntries() const LIFETIME_BOUND { return m_starExportEntries; }
    const Vector<WriteBarrier<AbstractModuleRecord>>& asyncParentModules() const LIFETIME_BOUND { return m_asyncParentModules; }
    CyclicModuleRecord* cycleRoot() const { return m_cycleRoot.get(); }
    AsyncEvaluationOrder asyncEvaluationOrder() const { return m_asyncEvaluationOrder; }
    std::optional<int> pendingAsyncDependencies() const { return m_pendingAsyncDependencies; }
    bool hasTLA() const { return m_hasTLA; }

    JSPromise* topLevelCapability() const { return m_topLevelCapability.get(); }
    void setCycleRoot(VM&, CyclicModuleRecord*);
    void setAsyncEvaluationOrder(AsyncEvaluationOrder newOrder) { m_asyncEvaluationOrder = newOrder; }
    void setPendingAsyncDependencies(std::optional<int> newDependencies) { m_pendingAsyncDependencies = newDependencies; }

    void appendAsyncParentModule(VM&, AbstractModuleRecord*);
    void setTopLevelCapability(VM&, JSPromise*);
    void setHasTLA(bool);

    void dump();

    struct Resolution {
        enum class Type { Resolved, NotFound, Ambiguous, Error };

        static Resolution NODELETE notFound();
        static Resolution NODELETE error();
        static Resolution NODELETE ambiguous();

        Type type;
        AbstractModuleRecord* moduleRecord;
        Identifier localName;
    };

    Resolution resolveExport(JSGlobalObject*, const Identifier& exportName);
    Resolution resolveImport(JSGlobalObject*, const Identifier& localName);

    AbstractModuleRecord* hostResolveImportedModule(JSGlobalObject*, const Identifier& moduleName);

    JSModuleNamespaceObject* getModuleNamespace(JSGlobalObject*);

    JSPromise* asyncCapability() const;
    void asyncCapability(VM&, JSPromise*);
    
    JSModuleEnvironment* moduleEnvironment()
    {
        ASSERT(m_moduleEnvironment);
        return m_moduleEnvironment.get();
    }

    JSModuleEnvironment* moduleEnvironmentMayBeNull()
    {
        return m_moduleEnvironment.get();
    }

    void link(JSGlobalObject*, RefPtr<ScriptFetcher> = nullptr);
    JS_EXPORT_PRIVATE JSValue evaluate(JSGlobalObject*, JSValue sentValue, JSValue resumeMode);
    WriteBarrier<Unknown>& internalField(Field field) { return Base::internalField(static_cast<uint32_t>(field)); }
    WriteBarrier<Unknown> internalField(Field field) const { return Base::internalField(static_cast<uint32_t>(field)); }

    void evaluateModuleSync(JSGlobalObject*);
    unsigned innerModuleEvaluation(JSGlobalObject*, Vector<AbstractModuleRecord*, 8>& stack, unsigned index);
    unsigned innerModuleLinking(JSGlobalObject*, Vector<CyclicModuleRecord*, 8>& stack, unsigned index, RefPtr<ScriptFetcher>);

    DECLARE_VISIT_CHILDREN;

    JSPromise* evaluate(JSGlobalObject*);

protected:
    AbstractModuleRecord(VM&, Structure*, Identifier);
    void finishCreation(JSGlobalObject*, VM&);

    void setModuleEnvironment(JSGlobalObject*, JSModuleEnvironment*);

private:
    struct ResolveQuery;
    static Resolution resolveExportImpl(JSGlobalObject*, const ResolveQuery&);
    std::optional<Resolution> NODELETE tryGetCachedResolution(UniquedStringImpl* exportName);
    void cacheResolution(UniquedStringImpl* exportName, const Resolution&);

    // The loader resolves the given module name to the module key. The module key is the unique value to represent this module.
    Identifier m_moduleKey;

    // Currently, we don't keep the occurrence order of the import / export entries.
    // So, we does not guarantee the order of the errors.
    // e.g. The import declaration that occurs later than another import declaration may
    //      throw the error even if the former import declaration also has the invalid content.
    //
    //      import ... // (1) this has some invalid content.
    //      import ... // (2) this also has some invalid content.
    //
    //      In the above case, (2) may throw the error earlier than (1)
    //
    // But, in all cases, we will throw the syntax error. So except for the content of the syntax error,
    // there are no differences.

    // Map localName -> ImportEntry.
    ImportEntries m_importEntries;

    // Map exportName -> ExportEntry.
    ExportEntries m_exportEntries;

    // Save the occurrence order since resolveExport requires it.
    OrderedIdentifierSet m_starExportEntries;

    // Save the occurrence order since the module loader loads and runs the modules in this order.
    // http://www.ecma-international.org/ecma-262/6.0/#sec-moduleevaluation
    Vector<ModuleRequest> m_requestedModules;

    WriteBarrier<JSModuleNamespaceObject> m_moduleNamespaceObject;

    WriteBarrier<JSPromise> m_asyncCapability;

    // We assume that all the AbstractModuleRecord are retained by JSModuleLoader's registry.
    // So here, we don't visit each object for GC. The resolution cache map caches the once
    // looked up correctly resolved resolution, since (1) we rarely looked up the non-resolved one,
    // and (2) if we cache all the attempts the size of the map becomes infinitely large.
    typedef UncheckedKeyHashMap<RefPtr<UniquedStringImpl>, Resolution, IdentifierRepHash, HashTraits<RefPtr<UniquedStringImpl>>> Resolutions;
    Resolutions m_resolutionCache;

protected:
    WriteBarrier<JSModuleEnvironment> m_moduleEnvironment;

    ModuleMap<LoadedModuleRequest> m_loadedModules;

    Vector<WriteBarrier<AbstractModuleRecord>> m_asyncParentModules;

    WriteBarrier<CyclicModuleRecord> m_cycleRoot;

    AsyncEvaluationOrder m_asyncEvaluationOrder { };

    UncheckedKeyHashMap<String, WriteBarrier<AbstractModuleRecord>> m_dependencies;

    WriteBarrier<JSPromise> m_topLevelCapability;

    std::optional<int> m_pendingAsyncDependencies;

    bool m_hasTLA { false };
};

} // namespace JSC
