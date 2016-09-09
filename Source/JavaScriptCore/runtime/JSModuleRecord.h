/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef JSModuleRecord_h
#define JSModuleRecord_h

#include "Identifier.h"
#include "JSDestructibleObject.h"
#include "SourceCode.h"
#include "VariableEnvironment.h"
#include <wtf/HashMap.h>
#include <wtf/ListHashSet.h>
#include <wtf/Optional.h>

namespace JSC {

class JSModuleNamespaceObject;
class JSModuleEnvironment;
class JSMap;
class ModuleProgramExecutable;

// Based on the Source Text Module Record
// http://www.ecma-international.org/ecma-262/6.0/#sec-source-text-module-records
class JSModuleRecord : public JSDestructibleObject {
    friend class LLIntOffsetsExtractor;
public:
    typedef JSDestructibleObject Base;

    // https://tc39.github.io/ecma262/#sec-source-text-module-records
    struct ExportEntry {
        enum class Type {
            Local,
            Indirect
        };

        static ExportEntry createLocal(const Identifier& exportName, const Identifier& localName);
        static ExportEntry createIndirect(const Identifier& exportName, const Identifier& importName, const Identifier& moduleName);

        Type type;
        Identifier exportName;
        Identifier moduleName;
        Identifier importName;
        Identifier localName;
    };

    struct ImportEntry {
        Identifier moduleRequest;
        Identifier importName;
        Identifier localName;

        bool isNamespace(VM& vm) const
        {
            return importName == vm.propertyNames->timesIdentifier;
        }
    };

    typedef WTF::ListHashSet<RefPtr<UniquedStringImpl>, IdentifierRepHash> OrderedIdentifierSet;
    typedef HashMap<RefPtr<UniquedStringImpl>, ImportEntry, IdentifierRepHash, HashTraits<RefPtr<UniquedStringImpl>>> ImportEntries;
    typedef HashMap<RefPtr<UniquedStringImpl>, ExportEntry, IdentifierRepHash, HashTraits<RefPtr<UniquedStringImpl>>> ExportEntries;

    DECLARE_EXPORT_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    static JSModuleRecord* create(ExecState* exec, VM& vm, Structure* structure, const Identifier& moduleKey, const SourceCode& sourceCode, const VariableEnvironment& declaredVariables, const VariableEnvironment& lexicalVariables)
    {
        JSModuleRecord* instance = new (NotNull, allocateCell<JSModuleRecord>(vm.heap)) JSModuleRecord(vm, structure, moduleKey, sourceCode, declaredVariables, lexicalVariables);
        instance->finishCreation(exec, vm);
        return instance;
    }

    void appendRequestedModule(const Identifier&);
    void addStarExportEntry(const Identifier&);
    void addImportEntry(const ImportEntry&);
    void addExportEntry(const ExportEntry&);

    Optional<ImportEntry> tryGetImportEntry(UniquedStringImpl* localName);
    Optional<ExportEntry> tryGetExportEntry(UniquedStringImpl* exportName);

    const SourceCode& sourceCode() const { return m_sourceCode; }
    const Identifier& moduleKey() const { return m_moduleKey; }
    const OrderedIdentifierSet& requestedModules() const { return m_requestedModules; }
    const ExportEntries& exportEntries() const { return m_exportEntries; }
    const ImportEntries& importEntries() const { return m_importEntries; }
    const OrderedIdentifierSet& starExportEntries() const { return m_starExportEntries; }

    const VariableEnvironment& declaredVariables() const { return m_declaredVariables; }
    const VariableEnvironment& lexicalVariables() const { return m_lexicalVariables; }

    void dump();

    JSModuleEnvironment* moduleEnvironment()
    {
        ASSERT(m_moduleEnvironment);
        return m_moduleEnvironment.get();
    }

    void link(ExecState*);
    JS_EXPORT_PRIVATE JSValue evaluate(ExecState*);

    ModuleProgramExecutable* moduleProgramExecutable() const { return m_moduleProgramExecutable.get(); }

    struct Resolution {
        enum class Type { Resolved, NotFound, Ambiguous, Error };

        static Resolution notFound();
        static Resolution error();
        static Resolution ambiguous();

        Type type;
        JSModuleRecord* moduleRecord;
        Identifier localName;
    };

    Resolution resolveExport(ExecState*, const Identifier& exportName);
    Resolution resolveImport(ExecState*, const Identifier& localName);

    JSModuleRecord* hostResolveImportedModule(ExecState*, const Identifier& moduleName);

private:
    JSModuleRecord(VM& vm, Structure* structure, const Identifier& moduleKey, const SourceCode& sourceCode, const VariableEnvironment& declaredVariables, const VariableEnvironment& lexicalVariables)
        : Base(vm, structure)
        , m_moduleKey(moduleKey)
        , m_sourceCode(sourceCode)
        , m_declaredVariables(declaredVariables)
        , m_lexicalVariables(lexicalVariables)
    {
    }

    void finishCreation(ExecState*, VM&);

    JSModuleNamespaceObject* getModuleNamespace(ExecState*);

    static void visitChildren(JSCell*, SlotVisitor&);
    static void destroy(JSCell*);

    void instantiateDeclarations(ExecState*, ModuleProgramExecutable*);

    struct ResolveQuery;
    static Resolution resolveExportImpl(ExecState*, const ResolveQuery&);
    Optional<Resolution> tryGetCachedResolution(UniquedStringImpl* exportName);
    void cacheResolution(UniquedStringImpl* exportName, const Resolution&);

    // The loader resolves the given module name to the module key. The module key is the unique value to represent this module.
    Identifier m_moduleKey;

    SourceCode m_sourceCode;

    VariableEnvironment m_declaredVariables;
    VariableEnvironment m_lexicalVariables;

    // Currently, we don't keep the occurrence order of the import / export entries.
    // So, we does not guarantee the order of the errors.
    // e.g. The import declaration that occurr later than the another import declaration may
    //      throw the error even if the former import declaration also has the invalid content.
    //
    //      import ... // (1) this has some invalid content.
    //      import ... // (2) this also has some invalid content.
    //
    //      In the above case, (2) may throw the error earlier than (1)
    //
    // But, in all the cases, we will throw the syntax error. So except for the content of the syntax error,
    // there are no difference.

    // Map localName -> ImportEntry.
    ImportEntries m_importEntries;

    // Map exportName -> ExportEntry.
    ExportEntries m_exportEntries;

    // Save the occurrence order since resolveExport requires it.
    OrderedIdentifierSet m_starExportEntries;

    // Save the occurrence order since the module loader loads and runs the modules in this order.
    // http://www.ecma-international.org/ecma-262/6.0/#sec-moduleevaluation
    OrderedIdentifierSet m_requestedModules;

    WriteBarrier<JSMap> m_dependenciesMap;

    WriteBarrier<ModuleProgramExecutable> m_moduleProgramExecutable;
    WriteBarrier<JSModuleEnvironment> m_moduleEnvironment;
    WriteBarrier<JSModuleNamespaceObject> m_moduleNamespaceObject;

    // We assume that all the JSModuleRecord are retained by JSModuleLoader's registry.
    // So here, we don't visit each object for GC. The resolution cache map caches the once
    // looked up correctly resolved resolution, since (1) we rarely looked up the non-resolved one,
    // and (2) if we cache all the attempts the size of the map becomes infinitely large.
    typedef HashMap<RefPtr<UniquedStringImpl>, Resolution, IdentifierRepHash, HashTraits<RefPtr<UniquedStringImpl>>> Resolutions;
    Resolutions m_resolutionCache;
};

} // namespace JSC

#endif // JSModuleRecord_h
