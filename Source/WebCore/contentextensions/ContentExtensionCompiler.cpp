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

#include "CombinedURLFilters.h"
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
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace ContentExtensions {

static void serializeSelector(Vector<SerializedActionByte>& actions, const String& selector)
{
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
        unsigned startIndex = actions.size();
        actions.resize(actions.size() + sizeof(UChar) * selectorLength);
        for (unsigned i = 0; i < selectorLength; ++i)
            *reinterpret_cast<UChar*>(&actions[startIndex + i * sizeof(UChar)]) = selector[i];
    } else {
        for (unsigned i = 0; i < selectorLength; ++i)
            actions.append(selector[i]);
    }
}
    
static Vector<unsigned> serializeActions(const Vector<ContentExtensionRule>& ruleList, Vector<SerializedActionByte>& actions)
{
    ASSERT(!actions.size());
    
    Vector<unsigned> actionLocations;
        
    for (unsigned ruleIndex = 0; ruleIndex < ruleList.size(); ++ruleIndex) {
        const ContentExtensionRule& rule = ruleList[ruleIndex];
        
        // Consolidate css selectors with identical triggers.
        if (rule.action().type() == ActionType::CSSDisplayNoneSelector) {
            StringBuilder selector;
            selector.append(rule.action().stringArgument());
            actionLocations.append(actions.size());
            for (unsigned i = ruleIndex + 1; i < ruleList.size(); i++) {
                if (rule.trigger() == ruleList[i].trigger() && ruleList[i].action().type() == ActionType::CSSDisplayNoneSelector) {
                    actionLocations.append(actions.size());
                    ruleIndex++;
                    selector.append(',');
                    selector.append(ruleList[i].action().stringArgument());
                } else
                    break;
            }
            serializeSelector(actions, selector.toString());
            continue;
        }
        
        // Identical sequential actions should not be rewritten.
        if (ruleIndex && rule.action() == ruleList[ruleIndex - 1].action()) {
            actionLocations.append(actionLocations[ruleIndex - 1]);
            continue;
        }

        actionLocations.append(actions.size());
        switch (rule.action().type()) {
        case ActionType::CSSDisplayNoneSelector:
        case ActionType::CSSDisplayNoneStyleSheet:
        case ActionType::InvalidAction:
            RELEASE_ASSERT_NOT_REACHED();

        case ActionType::BlockLoad:
        case ActionType::BlockCookies:
        case ActionType::IgnorePreviousRules:
            actions.append(static_cast<SerializedActionByte>(rule.action().type()));
            break;
        }
    }
    return actionLocations;
}

typedef HashSet<uint64_t, DefaultHash<uint64_t>::Hash, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>> UniversalActionLocationsSet;

static void addUniversalActionsToDFA(DFA& dfa, const UniversalActionLocationsSet& universalActionLocations)
{
    if (universalActionLocations.isEmpty())
        return;

    unsigned actionsStart = dfa.actions.size();
    dfa.actions.reserveCapacity(dfa.actions.size() + universalActionLocations.size());
    for (uint64_t actionLocation : universalActionLocations)
        dfa.actions.uncheckedAppend(actionLocation);
    unsigned actionsEnd = dfa.actions.size();
    unsigned actionsLength = actionsEnd - actionsStart;
    RELEASE_ASSERT_WITH_MESSAGE(actionsLength < std::numeric_limits<uint16_t>::max(), "Too many uncombined actions that match everything");
    dfa.nodes[dfa.root].setActions(actionsStart, static_cast<uint16_t>(actionsLength));
}

std::error_code compileRuleList(ContentExtensionCompilationClient& client, const String& ruleList)
{
    Vector<ContentExtensionRule> parsedRuleList;
    auto parserError = parseRuleList(ruleList, parsedRuleList);
    if (parserError)
        return parserError;

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    double patternPartitioningStart = monotonicallyIncreasingTime();
#endif

    Vector<SerializedActionByte> actions;
    Vector<unsigned> actionLocations = serializeActions(parsedRuleList, actions);
    client.writeActions(WTF::move(actions));
    LOG_LARGE_STRUCTURES(actions, actions.capacity() * sizeof(SerializedActionByte));
    actions.clear();

    UniversalActionLocationsSet universalActionLocations;

    CombinedURLFilters combinedURLFilters;
    URLFilterParser urlFilterParser(combinedURLFilters);
    bool ignorePreviousRulesSeen = false;
    for (unsigned ruleIndex = 0; ruleIndex < parsedRuleList.size(); ++ruleIndex) {
        const ContentExtensionRule& contentExtensionRule = parsedRuleList[ruleIndex];
        const Trigger& trigger = contentExtensionRule.trigger();
        ASSERT(trigger.urlFilter.length());

        // High bits are used for flags. This should match how they are used in DFABytecodeCompiler::compileNode.
        uint64_t actionLocationAndFlags = (static_cast<uint64_t>(trigger.flags) << 32) | static_cast<uint64_t>(actionLocations[ruleIndex]);
        URLFilterParser::ParseStatus status = urlFilterParser.addPattern(trigger.urlFilter, trigger.urlFilterIsCaseSensitive, actionLocationAndFlags);

        if (status == URLFilterParser::MatchesEverything) {
            if (ignorePreviousRulesSeen)
                return ContentExtensionError::RegexMatchesEverythingAfterIgnorePreviousRules;
            universalActionLocations.add(actionLocationAndFlags);
        }

        if (status != URLFilterParser::Ok && status != URLFilterParser::MatchesEverything) {
            dataLogF("Error while parsing %s: %s\n", trigger.urlFilter.utf8().data(), URLFilterParser::statusString(status).utf8().data());
            return ContentExtensionError::JSONInvalidRegex;
        }
        
        if (contentExtensionRule.action().type() == ActionType::IgnorePreviousRules)
            ignorePreviousRulesSeen = true;
    }
    LOG_LARGE_STRUCTURES(parsedRuleList, parsedRuleList.capacity() * sizeof(ContentExtensionRule)); // Doesn't include strings.
    LOG_LARGE_STRUCTURES(actionLocations, actionLocations.capacity() * sizeof(unsigned));
    parsedRuleList.clear();
    actionLocations.clear();

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    double patternPartitioningEnd = monotonicallyIncreasingTime();
    dataLogF("    Time spent partitioning the rules into groups: %f\n", (patternPartitioningEnd - patternPartitioningStart));
#endif

    LOG_LARGE_STRUCTURES(combinedURLFilters, combinedURLFilters.memoryUsed());

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    double totalNFAToByteCodeBuildTimeStart = monotonicallyIncreasingTime();
#endif

    bool firstNFASeen = false;
    combinedURLFilters.processNFAs([&](NFA&& nfa) {
#if CONTENT_EXTENSIONS_STATE_MACHINE_DEBUGGING
        nfa.debugPrintDot();
#endif

        LOG_LARGE_STRUCTURES(nfa, nfa.memoryUsed());

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
        double dfaBuildTimeStart = monotonicallyIncreasingTime();
#endif
        DFA dfa = NFAToDFA::convert(nfa);
        LOG_LARGE_STRUCTURES(dfa, dfa.memoryUsed());

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
        double dfaBuildTimeEnd = monotonicallyIncreasingTime();
        dataLogF("    Time spent building the DFA: %f\n", (dfaBuildTimeEnd - dfaBuildTimeStart));
#endif

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
        double dfaMinimizationTimeStart = monotonicallyIncreasingTime();
#endif
        dfa.minimize();
#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
        double dfaMinimizationTimeEnd = monotonicallyIncreasingTime();
        dataLogF("    Time spent miniminizing the DFA: %f\n", (dfaMinimizationTimeEnd - dfaMinimizationTimeStart));
#endif

#if CONTENT_EXTENSIONS_STATE_MACHINE_DEBUGGING
        WTFLogAlways("DFA");
        dfa.debugPrintDot();
#endif
        ASSERT_WITH_MESSAGE(!dfa.nodes[dfa.root].hasActions(), "All actions on the DFA root should come from regular expressions that match everything.");

        if (!firstNFASeen) {
            // Put all the universal actions on the first DFA.
            addUniversalActionsToDFA(dfa, universalActionLocations);
        }

        Vector<DFABytecode> bytecode;
        DFABytecodeCompiler compiler(dfa, bytecode);
        compiler.compile();
        LOG_LARGE_STRUCTURES(bytecode, bytecode.capacity() * sizeof(uint8_t));
        client.writeBytecode(WTF::move(bytecode));

        firstNFASeen = true;
    });

    if (!firstNFASeen) {
        // Our bytecode interpreter expects to have at least one DFA, so if we haven't seen any
        // create a dummy one and add any universal actions.

        NFA dummyNFA;
        DFA dummyDFA = NFAToDFA::convert(dummyNFA);

        addUniversalActionsToDFA(dummyDFA, universalActionLocations);

        Vector<DFABytecode> bytecode;
        DFABytecodeCompiler compiler(dummyDFA, bytecode);
        compiler.compile();
        LOG_LARGE_STRUCTURES(bytecode, bytecode.capacity() * sizeof(uint8_t));
        client.writeBytecode(WTF::move(bytecode));
    }

    // FIXME: combinedURLFilters should be cleared incrementally as it is processing NFAs. 
    combinedURLFilters.clear();

    LOG_LARGE_STRUCTURES(universalActionLocations, universalActionLocations.capacity() * sizeof(unsigned));
    universalActionLocations.clear();

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    double totalNFAToByteCodeBuildTimeEnd = monotonicallyIncreasingTime();
    dataLogF("    Time spent building and compiling the DFAs: %f\n", (totalNFAToByteCodeBuildTimeEnd - totalNFAToByteCodeBuildTimeStart));
    dataLogF("    Bytecode size %zu\n", bytecode.size());
#endif

    client.finalize();

    return { };
}

} // namespace ContentExtensions
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
