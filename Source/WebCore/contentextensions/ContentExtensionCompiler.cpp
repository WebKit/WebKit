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
#include "ContentExtensionError.h"
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
        case ActionType::CSSDisplayNoneStyleSheet:
        case ActionType::InvalidAction:
            RELEASE_ASSERT_NOT_REACHED();

        case ActionType::BlockLoad:
        case ActionType::BlockCookies:
        case ActionType::IgnorePreviousRules:
            actions.append(static_cast<SerializedActionByte>(rule.action().type()));
            break;

        case ActionType::CSSDisplayNoneSelector: {
            const String& selector = rule.action().stringArgument();
            // Append action type (1 byte).
            actions.append(static_cast<SerializedActionByte>(ActionType::CSSDisplayNoneSelector));
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


std::error_code compileRuleList(ContentExtensionCompilationClient& client, const String& ruleList)
{
    Vector<ContentExtensionRule> parsedRuleList;
    auto parserError = parseRuleList(ruleList, parsedRuleList);
    if (parserError)
        return parserError;

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    double nfaBuildTimeStart = monotonicallyIncreasingTime();
#endif

    Vector<SerializedActionByte> actions;
    Vector<unsigned> actionLocations = serializeActions(parsedRuleList, actions);
    Vector<uint64_t> universalActionLocations;

    Vector<NFA> nfas;
    nfas.append(NFA());
    bool nonUniversalActionSeen = false;
    for (unsigned ruleIndex = 0; ruleIndex < parsedRuleList.size(); ++ruleIndex) {

        // FIXME: Tune this better and adjust ContentExtensionTest.MultiDFA accordingly.
        if (nfas[nfas.size() - 1].graphSize() > 400)
            nfas.append(NFA());

        NFA& lastNFA = nfas[nfas.size() - 1];
        URLFilterParser urlFilterParser(lastNFA);
        const ContentExtensionRule& contentExtensionRule = parsedRuleList[ruleIndex];
        const Trigger& trigger = contentExtensionRule.trigger();
        ASSERT(trigger.urlFilter.length());

        // High bits are used for flags. This should match how they are used in DFABytecodeCompiler::compileNode.
        uint64_t actionLocationAndFlags = (static_cast<uint64_t>(trigger.flags) << 32) | static_cast<uint64_t>(actionLocations[ruleIndex]);
        URLFilterParser::ParseStatus status = urlFilterParser.addPattern(trigger.urlFilter, trigger.urlFilterIsCaseSensitive, actionLocationAndFlags);

        if (status == URLFilterParser::MatchesEverything) {
            if (nonUniversalActionSeen)
                dataLogF("Trigger matching everything found not at beginning.  This may cause incorrect behavior with ignore-previous-rules");
            universalActionLocations.append(actionLocationAndFlags);
        } else
            nonUniversalActionSeen = true;
        
        if (status != URLFilterParser::Ok && status != URLFilterParser::MatchesEverything) {
            dataLogF("Error while parsing %s: %s\n", trigger.urlFilter.utf8().data(), URLFilterParser::statusString(status).utf8().data());
            continue;
        }
    }

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    double nfaBuildTimeEnd = monotonicallyIncreasingTime();
    dataLogF("    Time spent building the NFA: %f\n", (nfaBuildTimeEnd - nfaBuildTimeStart));
#endif

#if CONTENT_EXTENSIONS_STATE_MACHINE_DEBUGGING
    for (size_t i = 0; i < nfas.size(); ++i) {
        WTFLogAlways("NFA %zu", i);
        const NFA& nfa = nfas[i];
        nfa.debugPrintDot();
    }
#endif

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    double dfaBuildTimeStart = monotonicallyIncreasingTime();
#endif
    Vector<DFABytecode> bytecode;
    for (size_t i = 0; i < nfas.size(); ++i) {
        DFA dfa = NFAToDFA::convert(nfas[i]);

#if CONTENT_EXTENSIONS_STATE_MACHINE_DEBUGGING
        WTFLogAlways("DFA %zu", i);
        dfa.debugPrintDot();
#endif

        if (!i) {
            // Put all the universal actions on the first DFA.
            for (uint64_t actionLocation : universalActionLocations)
                dfa.nodeAt(dfa.root()).actions.append(actionLocation);
        }
        DFABytecodeCompiler compiler(dfa, bytecode);
        compiler.compile();
    }

#if CONTENT_EXTENSIONS_STATE_MACHINE_DEBUGGING
    dfa.debugPrintDot();
#endif

    client.writeBytecode(WTF::move(bytecode));
    client.writeActions(WTF::move(actions));

    return { };
}

} // namespace ContentExtensions
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
