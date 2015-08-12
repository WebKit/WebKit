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

#include "config.h"
#include "ModuleRecord.h"

#include "IdentifierInlines.h"

namespace JSC {


auto ModuleRecord::ExportEntry::createLocal(const Identifier& exportName, const Identifier& localName, const VariableEnvironmentEntry& variable) -> ExportEntry
{
    return ExportEntry { Type::Local, exportName, Identifier(), Identifier(), localName, variable };
}

auto ModuleRecord::ExportEntry::createNamespace(const Identifier& exportName, const Identifier& moduleName) -> ExportEntry
{
    return ExportEntry { Type::Namespace, exportName, moduleName, Identifier(), Identifier(), VariableEnvironmentEntry() };
}

auto ModuleRecord::ExportEntry::createIndirect(const Identifier& exportName, const Identifier& importName, const Identifier& moduleName) -> ExportEntry
{
    return ExportEntry { Type::Indirect, exportName, moduleName, importName, Identifier(), VariableEnvironmentEntry() };
}

static String printableName(const RefPtr<UniquedStringImpl>& uid)
{
    if (uid->isSymbol())
        return uid.get();
    return WTF::makeString("'", String(uid.get()), "'");
}

static String printableName(const Identifier& ident)
{
    return printableName(ident.impl());
}


void ModuleRecord::dump()
{
    dataLog("\nAnalyzing ModuleRecord\n");

    dataLog("    Dependencies: ", m_requestedModules.size(), " modules\n");
    for (const auto& moduleName : m_requestedModules)
        dataLog("      module(", printableName(moduleName), ")\n");

    dataLog("    Import: ", m_importEntries.size(), " entries\n");
    for (const auto& pair : m_importEntries)
        dataLog("      import(", printableName(pair.value.importName), "), local(", printableName(pair.value.localName), "), module(", printableName(pair.value.moduleRequest), ")\n");

    dataLog("    Export: ", m_exportEntries.size(), " entries\n");
    for (const auto& pair : m_exportEntries) {
        const ExportEntry& entry = pair.value;
        switch (entry.type) {
        case ExportEntry::Type::Local:
            dataLog("      [Local] ", "export(", printableName(entry.exportName), "), local(", printableName(entry.localName), ")\n");
            break;

        case ExportEntry::Type::Namespace:
            dataLog("      [Namespace] ", "export(", printableName(entry.exportName), "), module(", printableName(entry.moduleName), ")\n");
            break;

        case ExportEntry::Type::Indirect:
            dataLog("      [Indirect] ", "export(", printableName(entry.exportName), "), import(", printableName(entry.importName), "), module(", printableName(entry.moduleName), ")\n");
            break;
        }
    }
    for (const auto& moduleName : m_starExportEntries)
        dataLog("      [Star] module(", printableName(moduleName.get()), ")\n");
}

} // namespace JSC
