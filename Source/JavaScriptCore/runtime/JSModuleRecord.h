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
#include "VariableEnvironment.h"
#include <wtf/HashMap.h>
#include <wtf/ListHashSet.h>

namespace JSC {

// Based on the Source Text Module Record
// http://www.ecma-international.org/ecma-262/6.0/#sec-source-text-module-records
class JSModuleRecord : public JSDestructibleObject {
public:
    typedef JSDestructibleObject Base;

    struct ExportEntry {
        enum class Type {
            Local,
            Namespace,
            Indirect
        };

        static ExportEntry createLocal(const Identifier& exportName, const Identifier& localName, const VariableEnvironmentEntry&);
        static ExportEntry createNamespace(const Identifier& exportName, const Identifier& moduleName);
        static ExportEntry createIndirect(const Identifier& exportName, const Identifier& importName, const Identifier& moduleName);

        Type type;
        Identifier exportName;
        Identifier moduleName;
        Identifier importName;
        Identifier localName;
        VariableEnvironmentEntry variable;
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
    typedef HashMap<RefPtr<UniquedStringImpl>, ImportEntry, IdentifierRepHash, HashTraits<RefPtr<UniquedStringImpl>>> ImportMap;
    typedef HashMap<RefPtr<UniquedStringImpl>, ExportEntry, IdentifierRepHash, HashTraits<RefPtr<UniquedStringImpl>>> ExportMap;

    DECLARE_EXPORT_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    static JSModuleRecord* create(VM& vm, Structure* structure, const Identifier& moduleKey)
    {
        JSModuleRecord* instance = new (NotNull, allocateCell<JSModuleRecord>(vm.heap)) JSModuleRecord(vm, structure, moduleKey);
        instance->finishCreation(vm);
        return instance;
    }

    static JSModuleRecord* create(ExecState* exec, Structure* structure, const Identifier& moduleKey)
    {
        return create(exec->vm(), structure, moduleKey);
    }

    void appendRequestedModule(const Identifier&);
    void addStarExportEntry(const Identifier&);
    void addImportEntry(const ImportEntry&);
    void addExportEntry(const ExportEntry&);

    const ImportEntry& lookUpImportEntry(const RefPtr<UniquedStringImpl>& localName);

    const OrderedIdentifierSet& requestedModules() const { return m_requestedModules; }

    void dump();

private:
    JSModuleRecord(VM& vm, Structure* structure, const Identifier& moduleKey)
        : Base(vm, structure)
        , m_moduleKey(moduleKey)
    {
    }

    void finishCreation(VM&);

    static void destroy(JSCell*);

    // The loader resolves the given module name to the module key. The module key is the unique value to represent this module.
    Identifier m_moduleKey;

    // Map localName -> ImportEntry.
    ImportMap m_importEntries;

    // Map exportName -> ExportEntry.
    ExportMap m_exportEntries;

    IdentifierSet m_starExportEntries;

    // Save the occurrence order since the module loader loads and runs the modules in this order.
    // http://www.ecma-international.org/ecma-262/6.0/#sec-moduleevaluation
    OrderedIdentifierSet m_requestedModules;
};

inline void JSModuleRecord::appendRequestedModule(const Identifier& moduleName)
{
    m_requestedModules.add(moduleName.impl());
}

inline void JSModuleRecord::addImportEntry(const ImportEntry& entry)
{
    m_importEntries.add(entry.localName.impl(), entry);
}

inline void JSModuleRecord::addExportEntry(const ExportEntry& entry)
{
    m_exportEntries.add(entry.exportName.impl(), entry);
}

inline void JSModuleRecord::addStarExportEntry(const Identifier& moduleName)
{
    m_starExportEntries.add(moduleName.impl());
}

inline auto JSModuleRecord::lookUpImportEntry(const RefPtr<UniquedStringImpl>& localName) -> const ImportEntry&
{
    return (*m_importEntries.find(localName)).value;
}

} // namespace JSC

#endif // JSModuleRecord_h
