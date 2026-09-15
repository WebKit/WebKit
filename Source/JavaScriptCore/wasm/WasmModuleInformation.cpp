/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "WasmModuleInformation.h"
#include "WebAssemblyBuiltin.h"
#include "WebAssemblyCompileOptions.h"

#if ENABLE(WEBASSEMBLY)

#include "WasmNameSection.h"

#if ENABLE(WEBASSEMBLY_DEBUGGER)
#include "WasmIPIntGenerator.h"
#include "WasmModuleDebugInfo.h"
#include <wtf/URL.h>
#include <wtf/text/StringBuilder.h>
#endif

namespace JSC { namespace Wasm {

ModuleInformation::ModuleInformation()
    : m_nameSection(NameSection::create())
{
    m_nameSectionPtr.store(m_nameSection.ptr(), std::memory_order_relaxed);
#if ENABLE(WEBASSEMBLY_DEBUGGER)
    if (Options::enableWasmDebugger()) [[unlikely]]
        debugInfo = WTF::makeUnique<ModuleDebugInfo>();
#endif
}

ModuleInformation::~ModuleInformation() = default;

#if ENABLE(WEBASSEMBLY_DEBUGGER)
FunctionDebugInfo& ModuleInformation::ensureFunctionDebugInfo(FunctionCodeIndex functionIndex) const
{
    RELEASE_ASSERT(functionIndex < functions.size());

    auto iterator = debugInfo->functionIndexToData.find(functionIndex);
    if (iterator != debugInfo->functionIndexToData.end())
        return iterator->value;

    dataLogLnIf(Options::verboseWasmDebugger(), "[ModuleDebugInfo] Lazy collection for function ", functionIndex);
    const auto& function = functions[functionIndex];
    FunctionSpaceIndex spaceIndex = toSpaceIndex(functionIndex);
    Ref rtt = this->rtt(spaceIndex);
    auto& info = debugInfo->functionIndexToData.add(functionIndex, FunctionDebugInfo()).iterator->value;
    auto functionData = debugInfo->source.subspan(function.start, function.data.size());

    // parseForDebugInfo hands the module information to IPIntGenerator and FunctionParser, which
    // both take it non-const; nothing on this path mutates it. Same cast as WasmIPIntSlowPaths.cpp
    // makes for BBQPlan.
    parseForDebugInfo(functionData, rtt.get(), const_cast<ModuleInformation&>(*this), functionIndex, info);
    dataLogLnIf(Options::verboseWasmDebugger(), "[ModuleDebugInfo] Debug info collection completed for function ", functionIndex, " with ", info.offsetToNextInstructions.size(), " instruction mappings and ", info.locals.size(), " locals");
    return info;
}

bool ModuleInformation::isInstructionStart(uint32_t moduleOffset) const
{
#if ASSERT_ENABLED
    for (size_t i = 1; i < functions.size(); ++i)
        ASSERT(functions[i - 1].start <= functions[i].start);
#endif

    auto iterator = std::upper_bound(functions.begin(), functions.end(), moduleOffset,
        [](uint32_t offset, const FunctionData& function) {
            return offset < function.start;
        });
    if (iterator == functions.begin())
        return false;

    auto functionIterator = std::prev(iterator);
    const auto& function = *functionIterator;
    if (moduleOffset < function.start || moduleOffset >= function.end)
        return false;

    size_t functionIndex = std::distance(functions.begin(), functionIterator);
    const auto& starts = ensureFunctionDebugInfo(FunctionCodeIndex(functionIndex)).instructionStarts;
    ASSERT(!starts.isEmpty());
    return starts.contains(moduleOffset);
}

String ModuleInformation::declaredName() const
{
    if (debugInfo->cachedDeclaredName)
        return *debugInfo->cachedDeclaredName;

    StringBuilder result;

    if (!this->sourceURL.isEmpty()) {
        // LLDB normalizes "//" -> "/" in library names (FileSpec treats them as paths),
        // so we strip the URL scheme and store only "host/path" to avoid mangling.
        auto sourceURL = makeString(this->sourceURL);
        URL url { sourceURL };
        if (url.isValid() && !url.host().isEmpty())
            result.append(makeString(url.host(), url.path()));
        else
            result.append(sourceURL);
    }

    const auto& rawName = nameSection().moduleName;
    if (!rawName.isEmpty()) {
        if (!result.isEmpty())
            result.append(':');
        result.append(rawName.span());
    }

    debugInfo->cachedDeclaredName = result.toString();
    dataLogLnIf(Options::verboseWasmDebugger(), "[ModuleDebugInfo][declaredName] ", *debugInfo->cachedDeclaredName);
    return *debugInfo->cachedDeclaredName;
}
#endif


void ModuleInformation::setNameSection(Ref<NameSection>&& section)
{
    // The spec has the following editorial note:
    //   The name section should appear only once in a module, [...]
    //
    // Since custom sections have no effect on the observable behavior, for simplicity, we ignore
    // name sections after the first one if multiple such sections are present in a module.
    if (m_hasCustomNameSection)
        return;
    m_hasCustomNameSection = true;
    m_retiredNameSection = WTF::move(m_nameSection);
    m_nameSection = WTF::move(section);
    m_nameSectionPtr.store(m_nameSection.ptr(), std::memory_order_release);
}

static std::optional<Name> encodeAsImportName(const String& string)
{
    auto utf8 = string.tryGetUTF8(StrictConversion);
    if (!utf8)
        return std::nullopt;
    return Name(byteCast<char8_t>(utf8->span()));
}

// This is called during module creation, so at this point we have fully isolated access
// to this ModuleInformation object.
void ModuleInformation::applyCompileOptions(const WebAssemblyCompileOptions& options)
{
    const auto& constants = options.importedStringConstants();
    if (constants.has_value())
        m_importedStringConstants = encodeAsImportName(*constants);
    const auto& builtinSetNames = options.qualifiedBuiltinSetNames();
    for (const auto& name : builtinSetNames) {
        if (auto encoded = encodeAsImportName(name))
            m_qualifiedBuiltinSetNames.append(WTF::move(*encoded));
    }
    populateImportShouldBeHidden();
}

/**
 * Precompute a map indicating which of the imports should not appear in the
 * result of Module.imports() according to
 * https://webassembly.github.io/js-string-builtins/js-api/#dom-module-imports
 */
void ModuleInformation::populateImportShouldBeHidden()
{
    // The following would theoretically be a strict ==, but an inline FixedBitVector reports a larger size than it was created with.
    RELEASE_ASSERT(importShouldBeHidden.size() >= imports.size());
    for (size_t i = 0; i < imports.size(); ++i) {
        const Import& import = imports[i];

        if (importedStringConstantsEquals(import.module))
            importShouldBeHidden.testAndSet(i);
        else if (builtinSetsInclude(import.module)) {
            auto* builtinSet = WebAssemblyBuiltinRegistry::singleton().findByQualifiedName(makeString(import.module));
            if (builtinSet) {
                String fieldName = makeString(import.field);
                if (builtinSet->findBuiltin(fieldName))
                    importShouldBeHidden.testAndSet(i);
            }
        }
    }
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
