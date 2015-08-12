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

#ifndef ModuleRecord_h
#define ModuleRecord_h

#include "Nodes.h"
#include <wtf/HashMap.h>
#include <wtf/ListHashSet.h>

namespace JSC {

// Based on the Source Text Module Record
// http://www.ecma-international.org/ecma-262/6.0/#sec-source-text-module-records
class ModuleRecord : public RefCounted<ModuleRecord> {
public:
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

    static Ref<ModuleRecord> create()
    {
        return adoptRef(*new ModuleRecord);
    }

    void appendRequestedModule(const Identifier&);
    void addStarExportEntry(const Identifier&);
    void addImportEntry(const ImportEntry&);
    void addExportEntry(const ExportEntry&);

    const ImportEntry& lookUpImportEntry(const RefPtr<UniquedStringImpl>& localName);

    void dump();

private:
    // Map localName -> ImportEntry.
    ImportMap m_importEntries;

    // Map exportName -> ExportEntry.
    ExportMap m_exportEntries;

    IdentifierSet m_starExportEntries;

    // Save the occurrence order since the module loader loads and runs the modules in this order.
    // http://www.ecma-international.org/ecma-262/6.0/#sec-moduleevaluation
    OrderedIdentifierSet m_requestedModules;
};

inline void ModuleRecord::appendRequestedModule(const Identifier& moduleName)
{
    m_requestedModules.add(moduleName.impl());
}

inline void ModuleRecord::addImportEntry(const ImportEntry& entry)
{
    m_importEntries.add(entry.localName.impl(), entry);
}

inline void ModuleRecord::addExportEntry(const ExportEntry& entry)
{
    m_exportEntries.add(entry.exportName.impl(), entry);
}

inline void ModuleRecord::addStarExportEntry(const Identifier& moduleName)
{
    m_starExportEntries.add(moduleName.impl());
}

inline auto ModuleRecord::lookUpImportEntry(const RefPtr<UniquedStringImpl>& localName) -> const ImportEntry&
{
    return (*m_importEntries.find(localName)).value;
}

} // namespace JSC

#endif // ModuleRecord_h
