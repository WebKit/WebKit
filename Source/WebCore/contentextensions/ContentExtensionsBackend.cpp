/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "ContentExtensionsBackend.h"

#if ENABLE(CONTENT_EXTENSIONS)

#include "ContentExtensionsDebugging.h"
#include "DFABytecodeCompiler.h"
#include "DFABytecodeInterpreter.h"
#include "NFA.h"
#include "NFAToDFA.h"
#include "URL.h"
#include "URLFilterParser.h"
#include <wtf/CurrentTime.h>
#include <wtf/DataLog.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/CString.h>

namespace WebCore {

namespace ContentExtensions {
    
Vector<unsigned> ContentExtensionsBackend::serializeActions(const Vector<ContentExtensionRule>& ruleList, Vector<SerializedActionByte>& actions)
{
    ASSERT(!actions.size());
    
    Vector<unsigned> actionLocations;
        
    for (unsigned ruleIndex = 0; ruleIndex < ruleList.size(); ++ruleIndex) {
        const ContentExtensionRule& rule = ruleList[ruleIndex];
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

void ContentExtensionsBackend::setRuleList(const String& identifier, const Vector<ContentExtensionRule>& ruleList)
{
    ASSERT(!identifier.isEmpty());
    if (identifier.isEmpty())
        return;

    if (ruleList.isEmpty()) {
        removeRuleList(identifier);
        return;
    }

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    double nfaBuildTimeStart = monotonicallyIncreasingTime();
#endif

    Vector<SerializedActionByte> actions;
    Vector<unsigned> actionLocations = serializeActions(ruleList, actions);

    NFA nfa;
    URLFilterParser urlFilterParser(nfa);
    for (unsigned ruleIndex = 0; ruleIndex < ruleList.size(); ++ruleIndex) {
        const ContentExtensionRule& contentExtensionRule = ruleList[ruleIndex];
        const Trigger& trigger = contentExtensionRule.trigger();
        ASSERT(trigger.urlFilter.length());

        String error = urlFilterParser.addPattern(trigger.urlFilter, trigger.urlFilterIsCaseSensitive, actionLocations[ruleIndex]);

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
    CompiledContentExtension compiledContentExtension = { bytecode, actions };
    m_ruleLists.set(identifier, compiledContentExtension);
}

void ContentExtensionsBackend::removeRuleList(const String& identifier)
{
    m_ruleLists.remove(identifier);
}

void ContentExtensionsBackend::removeAllRuleLists()
{
    m_ruleLists.clear();
}

Vector<Action> ContentExtensionsBackend::actionsForURL(const URL& url)
{
    const String& urlString = url.string();
    ASSERT_WITH_MESSAGE(urlString.containsOnlyASCII(), "A decoded URL should only contain ASCII characters. The matching algorithm assumes the input is ASCII.");
    const CString& urlCString = urlString.utf8();

    Vector<Action> actions;
    for (auto& ruleListSlot : m_ruleLists) {
        const CompiledContentExtension& compiledContentExtension = ruleListSlot.value;
        DFABytecodeInterpreter interpreter(compiledContentExtension.bytecode);
        DFABytecodeInterpreter::Actions triggeredActions = interpreter.interpret(urlCString);
        
        if (!triggeredActions.isEmpty()) {
            Vector<unsigned> actionLocations;
            actionLocations.reserveInitialCapacity(triggeredActions.size());
            for (auto actionLocation : triggeredActions)
                actionLocations.append(static_cast<unsigned>(actionLocation));
            std::sort(actionLocations.begin(), actionLocations.end());
            
            // Add actions in reverse order to properly deal with IgnorePreviousRules.
            for (unsigned i = actionLocations.size(); i; i--) {
                Action action = Action::deserialize(ruleListSlot.value.actions, actionLocations[i - 1]);
                if (action.type() == ActionType::IgnorePreviousRules)
                    break;
                actions.append(action);
                if (action.type() == ActionType::BlockLoad)
                    break;
            }
        }
    }
    return actions;
}

} // namespace ContentExtensions

} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
