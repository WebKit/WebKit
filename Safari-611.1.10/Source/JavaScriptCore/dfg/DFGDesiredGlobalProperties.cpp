/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "DFGDesiredGlobalProperties.h"

#if ENABLE(DFG_JIT)

#include "CodeBlock.h"
#include "DFGCommonData.h"
#include "DFGDesiredIdentifiers.h"
#include "JSGlobalObject.h"

namespace JSC { namespace DFG {

bool DesiredGlobalProperties::isStillValidOnMainThread(VM& vm, DesiredIdentifiers& identifiers)
{
    bool isStillValid = true;
    for (const auto& property : m_set) {
        auto* uid = identifiers.at(property.identifierNumber());
        JSGlobalObject* globalObject = property.globalObject();
        {
            SymbolTable* symbolTable = globalObject->globalLexicalEnvironment()->symbolTable();
            ConcurrentJSLocker locker(symbolTable->m_lock);
            if (!symbolTable->contains(locker, uid))
                continue;
        }
        // Set invalidated WatchpointSet here to prevent further compile-and-fail loop.
        property.globalObject()->ensureReferencedPropertyWatchpointSet(uid).fireAll(vm, "Lexical binding shadows an existing global property");
        isStillValid = false;
    }
    return isStillValid;
}

void DesiredGlobalProperties::reallyAdd(CodeBlock* codeBlock, DesiredIdentifiers& identifiers, CommonData& common)
{
    for (const auto& property : m_set) {
        auto* uid = identifiers.at(property.identifierNumber());
        auto& watchpointSet = property.globalObject()->ensureReferencedPropertyWatchpointSet(uid);
        ASSERT(watchpointSet.isStillValid());
        CodeBlockJettisoningWatchpoint* watchpoint = nullptr;
        {
            ConcurrentJSLocker locker(codeBlock->m_lock);
            watchpoint = common.watchpoints.add(codeBlock);
        }
        watchpointSet.add(WTFMove(watchpoint));
    }
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

