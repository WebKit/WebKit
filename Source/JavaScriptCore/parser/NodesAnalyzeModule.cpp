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
#include "Nodes.h"

#include "JSCJSValueInlines.h"
#include "JSModuleRecord.h"
#include "ModuleAnalyzer.h"

namespace JSC {

static Expected<RefPtr<ScriptFetchParameters>, std::tuple<ErrorType, String>> tryCreateAttributes(VM& vm, ImportAttributesListNode* attributesList)
{
    if (!attributesList)
        return RefPtr<ScriptFetchParameters> { };

    // https://tc39.es/proposal-import-attributes/#sec-AllImportAttributesSupported
    // Currently, only "type" is supported.
    std::optional<ScriptFetchParameters::Type> type;
    for (auto& [key, value] : attributesList->attributes()) {
        if (*key != vm.propertyNames->type)
            return makeUnexpected(std::tuple { ErrorType::SyntaxError, makeString("Import attribute \""_s, StringView(key->impl()), "\" is not supported"_s) });
    }

    for (auto& [key, value] : attributesList->attributes()) {
        if (*key == vm.propertyNames->type) {
            type = ScriptFetchParameters::parseType(value->impl());
            if (!type)
                return makeUnexpected(std::tuple { ErrorType::TypeError, makeString("Import attribute type \""_s, StringView(value->impl()), "\" is not valid"_s) });
        }
    }

    if (type)
        return RefPtr<ScriptFetchParameters>(ScriptFetchParameters::create(type.value()));
    return RefPtr<ScriptFetchParameters> { };
}

bool ScopeNode::analyzeModule(ModuleAnalyzer& analyzer)
{
    return m_statements->analyzeModule(analyzer);
}

bool SourceElements::analyzeModule(ModuleAnalyzer& analyzer)
{
    // In the module analyzer phase, only module declarations are included in the top-level SourceElements.
    for (StatementNode* statement = m_head; statement; statement = statement->next()) {
        ASSERT(statement->isModuleDeclarationNode());
        if (!static_cast<ModuleDeclarationNode*>(statement)->analyzeModule(analyzer))
            return false;
    }
    return true;
}

bool ImportDeclarationNode::analyzeModule(ModuleAnalyzer& analyzer)
{
    auto result = tryCreateAttributes(analyzer.vm(), attributesList());
    if (!result) {
        analyzer.fail(WTFMove(result.error()));
        return false;
    }

    analyzer.appendRequestedModule(m_moduleName->moduleName(), WTFMove(result.value()));
    for (auto* specifier : m_specifierList->specifiers()) {
        analyzer.moduleRecord()->addImportEntry(JSModuleRecord::ImportEntry {
            specifier->importedName() == analyzer.vm().propertyNames->timesIdentifier
                ? JSModuleRecord::ImportEntryType::Namespace : JSModuleRecord::ImportEntryType::Single,
            m_moduleName->moduleName(),
            specifier->importedName(),
            specifier->localName(),
        });
    }
    return true;
}

bool ExportAllDeclarationNode::analyzeModule(ModuleAnalyzer& analyzer)
{
    auto result = tryCreateAttributes(analyzer.vm(), attributesList());
    if (!result) {
        analyzer.fail(WTFMove(result.error()));
        return false;
    }

    analyzer.appendRequestedModule(m_moduleName->moduleName(), WTFMove(result.value()));
    analyzer.moduleRecord()->addStarExportEntry(m_moduleName->moduleName());
    return true;
}

bool ExportDefaultDeclarationNode::analyzeModule(ModuleAnalyzer&)
{
    return true;
}

bool ExportLocalDeclarationNode::analyzeModule(ModuleAnalyzer&)
{
    return true;
}

bool ExportNamedDeclarationNode::analyzeModule(ModuleAnalyzer& analyzer)
{
    if (m_moduleName) {
        auto result = tryCreateAttributes(analyzer.vm(), attributesList());
        if (!result) {
            analyzer.fail(WTFMove(result.error()));
            return false;
        }

        analyzer.appendRequestedModule(m_moduleName->moduleName(), WTFMove(result.value()));
    }

    for (auto* specifier : m_specifierList->specifiers()) {
        if (m_moduleName) {
            // export { v } from "mod"
            //
            // In this case, no local variable names are imported into the current module.
            // "v" indirectly points the binding in "mod".
            //
            // export * as v from "mod"
            //
            // If it is namespace export, we should use createNamespace.
            if (specifier->localName() == analyzer.vm().propertyNames->starNamespacePrivateName)
                analyzer.moduleRecord()->addExportEntry(JSModuleRecord::ExportEntry::createNamespace(specifier->exportedName(), m_moduleName->moduleName()));
            else
                analyzer.moduleRecord()->addExportEntry(JSModuleRecord::ExportEntry::createIndirect(specifier->exportedName(), specifier->localName(), m_moduleName->moduleName()));
        }
    }
    return true;
}

} // namespace JSC
