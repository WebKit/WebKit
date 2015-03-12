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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ContentExtensionCompiler.h"

#if ENABLE(CONTENT_EXTENSIONS)

#include "CompiledContentExtension.h"
#include "ContentExtensionActions.h"
#include "ContentExtensionParser.h"
#include "ContentExtensionRule.h"
#include "ContentExtensionsDebugging.h"
#include "DFABytecodeCompiler.h"
#include "NFA.h"
#include "NFAToDFA.h"
#include "URLFilterParser.h"
#include <wtf/CurrentTime.h>
#include <wtf/DataLog.h>
#include <wtf/text/CString.h>

namespace WebCore {
namespace ContentExtensions {

static Vector<unsigned> serializeActions(const Vector<ContentExtensionRule>& ruleList, Vector<SerializedActionByte>& actions)
{
    ASSERT(!actions.size());
    
    Vector<unsigned> actionLocations;
        
    for (unsigned ruleIndex = 0; ruleIndex < ruleList.size(); ++ruleIndex) {
        const ContentExtensionRule& rule = ruleList[ruleIndex];
        
        // Identical sequential actions should not be rewritten.
        if (ruleIndex && rule.action() == ruleList[ruleIndex - 1].action()) {
            actionLocations.append(actionLocations[ruleIndex - 1]);
            continue;
        }
        actionLocations.append(actions.size());
        
        switch (rule.action().type()) {
        case ActionType::InvalidAction:
            RELEASE_ASSERT_NOT_REACHED();

        case ActionType::BlockLoad:
        case ActionType::BlockCookies:
        case ActionType::IgnorePreviousRules:
            actions.append(static_cast<SerializedActionByte>(rule.action().type()));
            break;

        case ActionType::CSSDisplayNone: {
            const String& selector = rule.action().cssSelector();
            // Append action type (1 byte).
            actions.append(static_cast<SerializedActionByte>(ActionType::CSSDisplayNone));
            // Append Selector length (4 bytes).
            unsigned selectorLength = selector.length();
            actions.resize(actions.size() + sizeof(unsigned));
            *reinterpret_cast<unsigned*>(&actions[actions.size() - sizeof(unsigned)]) = selectorLength;
            bool wideCharacters = !selector.is8Bit();
            actions.append(wideCharacters);
            // Append Selector.
            if (wideCharacters) {
                for (unsigned i = 0; i < selectorLength; i++) {
                    actions.resize(actions.size() + sizeof(UChar));
                    *reinterpret_cast<UChar*>(&actions[actions.size() - sizeof(UChar)]) = selector[i];
                }
            } else {
                for (unsigned i = 0; i < selectorLength; i++)
                    actions.append(selector[i]);
            }
            break;
        }
        }
    }
    return actionLocations;
}


CompiledContentExtensionData compileRuleList(const String& ruleList)
{
    auto parsedRuleList = parseRuleList(ruleList);

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    double nfaBuildTimeStart = monotonicallyIncreasingTime();
#endif

    Vector<SerializedActionByte> actions;
    Vector<unsigned> actionLocations = serializeActions(parsedRuleList, actions);

    NFA nfa;
    URLFilterParser urlFilterParser(nfa);
    for (unsigned ruleIndex = 0; ruleIndex < parsedRuleList.size(); ++ruleIndex) {
        const ContentExtensionRule& contentExtensionRule = parsedRuleList[ruleIndex];
        const Trigger& trigger = contentExtensionRule.trigger();
        ASSERT(trigger.urlFilter.length());

        // High bits are used for flags. This should match how they are used in DFABytecodeCompiler::compileNode.
        String error = urlFilterParser.addPattern(trigger.urlFilter, trigger.urlFilterIsCaseSensitive, (static_cast<uint64_t>(trigger.flags) << 32) | static_cast<uint64_t>(actionLocations[ruleIndex]));

        if (!error.isNull()) {
            dataLogF("Error while parsing %s: %s\n", trigger.urlFilter.utf8().data(), error.utf8().data());
            continue;
        }
    }

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    double nfaBuildTimeEnd = monotonicallyIncreasingTime();
    dataLogF("    Time spent building the NFA: %f\n", (nfaBuildTimeEnd - nfaBuildTimeStart));
#endif

#if CONTENT_EXTENSIONS_STATE_MACHINE_DEBUGGING
    nfa.debugPrintDot();
#endif

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    double dfaBuildTimeStart = monotonicallyIncreasingTime();
#endif

    const DFA dfa = NFAToDFA::convert(nfa);

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    double dfaBuildTimeEnd = monotonicallyIncreasingTime();
    dataLogF("    Time spent building the DFA: %f\n", (dfaBuildTimeEnd - dfaBuildTimeStart));
#endif

    // FIXME: never add a DFA that only matches the empty set.

#if CONTENT_EXTENSIONS_STATE_MACHINE_DEBUGGING
    dfa.debugPrintDot();
#endif

    Vector<DFABytecode> bytecode;
    DFABytecodeCompiler compiler(dfa, bytecode);
    compiler.compile();

    return { WTF::move(bytecode), WTF::move(actions) };
}

} // namespace ContentExtensions
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
