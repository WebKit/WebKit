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
        
        // Identical sequential actions should not be rewritten unless there are domains in the trigger.
        // If there are domains in the trigger, we need to distinguish the actions by index to tell if we need to apply it
        // by comparing the output of the filters with domains and the domain filters.
        if (ruleIndex && rule.action() == ruleList[ruleIndex - 1].action() && rule.trigger().domains.isEmpty()) {
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

typedef HashSet<uint64_t, DefaultHash<uint64_t>::Hash, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>> UniversalActionSet;

static void addUniversalActionsToDFA(DFA& dfa, const UniversalActionSet& universalActions)
{
    if (universalActions.isEmpty())
        return;

    DFANode& root = dfa.nodes[dfa.root];
    ASSERT(!root.actionsLength());
    unsigned actionsStart = dfa.actions.size();
    dfa.actions.reserveCapacity(dfa.actions.size() + universalActions.size());
    for (uint64_t action : universalActions)
        dfa.actions.uncheckedAppend(action);
    unsigned actionsEnd = dfa.actions.size();

    unsigned actionsLength = actionsEnd - actionsStart;
    RELEASE_ASSERT_WITH_MESSAGE(actionsLength < std::numeric_limits<uint16_t>::max(), "Too many uncombined actions that match everything");
    root.setActions(actionsStart, static_cast<uint16_t>(actionsLength));
}

std::error_code compileRuleList(ContentExtensionCompilationClient& client, String&& ruleList)
{
    Vector<ContentExtensionRule> parsedRuleList;
    auto parserError = parseRuleList(ruleList, parsedRuleList);
    ruleList = String();
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

    UniversalActionSet universalActionsWithoutDomains;
    UniversalActionSet universalActionsWithDomains;

    // FIXME: These don't all need to be in memory at the same time.
    CombinedURLFilters filtersWithoutDomains;
    CombinedURLFilters filtersWithDomains;
    CombinedURLFilters domainFilters;
    URLFilterParser filtersWithoutDomainParser(filtersWithoutDomains);
    URLFilterParser filtersWithDomainParser(filtersWithDomains);
    
    bool ignorePreviousRulesSeen = false;
    for (unsigned ruleIndex = 0; ruleIndex < parsedRuleList.size(); ++ruleIndex) {
        const ContentExtensionRule& contentExtensionRule = parsedRuleList[ruleIndex];
        const Trigger& trigger = contentExtensionRule.trigger();
        const Action& action = contentExtensionRule.action();
        ASSERT(trigger.urlFilter.length());

        // High bits are used for flags. This should match how they are used in DFABytecodeCompiler::compileNode.
        uint64_t actionLocationAndFlags = (static_cast<uint64_t>(trigger.flags) << 32) | static_cast<uint64_t>(actionLocations[ruleIndex]);
        URLFilterParser::ParseStatus status = URLFilterParser::Ok;
        if (trigger.domains.isEmpty()) {
            ASSERT(trigger.domainCondition == Trigger::DomainCondition::None);
            status = filtersWithoutDomainParser.addPattern(trigger.urlFilter, trigger.urlFilterIsCaseSensitive, actionLocationAndFlags);
            if (status == URLFilterParser::MatchesEverything) {
                if (!ignorePreviousRulesSeen
                    && trigger.domainCondition == Trigger::DomainCondition::None
                    && action.type() == ActionType::CSSDisplayNoneSelector
                    && !trigger.flags)
                    actionLocationAndFlags |= DisplayNoneStyleSheetFlag;
                universalActionsWithoutDomains.add(actionLocationAndFlags);
                status = URLFilterParser::Ok;
            }
            if (status != URLFilterParser::Ok) {
                dataLogF("Error while parsing %s: %s\n", trigger.urlFilter.utf8().data(), URLFilterParser::statusString(status).utf8().data());
                return ContentExtensionError::JSONInvalidRegex;
            }
        } else {
            if (trigger.domainCondition == Trigger::DomainCondition::IfDomain)
                actionLocationAndFlags |= IfDomainFlag;
            else {
                ASSERT(trigger.domainCondition == Trigger::DomainCondition::UnlessDomain);
                ASSERT(!(actionLocationAndFlags & IfDomainFlag));
            }
            
            status = filtersWithDomainParser.addPattern(trigger.urlFilter, trigger.urlFilterIsCaseSensitive, actionLocationAndFlags);
            if (status == URLFilterParser::MatchesEverything) {
                universalActionsWithDomains.add(actionLocationAndFlags);
                status = URLFilterParser::Ok;
            }
            if (status != URLFilterParser::Ok) {
                dataLogF("Error while parsing %s: %s\n", trigger.urlFilter.utf8().data(), URLFilterParser::statusString(status).utf8().data());
                return ContentExtensionError::JSONInvalidRegex;
            }
            for (const String& domain : trigger.domains)
                domainFilters.addDomain(actionLocationAndFlags, domain);
        }
        ASSERT(status == URLFilterParser::Ok);
        
        if (action.type() == ActionType::IgnorePreviousRules)
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

    LOG_LARGE_STRUCTURES(filtersWithoutDomains, filtersWithoutDomains.memoryUsed());
    LOG_LARGE_STRUCTURES(filtersWithDomains, filtersWithDomains.memoryUsed());
    LOG_LARGE_STRUCTURES(domainFilters, domainFilters.memoryUsed());

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    double totalNFAToByteCodeBuildTimeStart = monotonicallyIncreasingTime();
#endif

    // Smaller maxNFASizes risk high compiling and interpreting times from having too many DFAs,
    // larger maxNFASizes use too much memory when compiling.
    const unsigned maxNFASize = 30000;
    
    bool firstNFAWithoutDomainsSeen = false;
    // FIXME: Combine small NFAs to reduce the number of NFAs.
    filtersWithoutDomains.processNFAs(maxNFASize, [&](NFA&& nfa) {
#if CONTENT_EXTENSIONS_STATE_MACHINE_DEBUGGING
        dataLogF("filtersWithoutDomains NFA\n");
        nfa.debugPrintDot();
#endif

        LOG_LARGE_STRUCTURES(nfa, nfa.memoryUsed());

        DFA dfa = NFAToDFA::convert(nfa);
        LOG_LARGE_STRUCTURES(dfa, dfa.memoryUsed());

        dfa.minimize();

#if CONTENT_EXTENSIONS_STATE_MACHINE_DEBUGGING
        dataLogF("filtersWithoutDomains DFA\n");
        dfa.debugPrintDot();
#endif
        ASSERT_WITH_MESSAGE(!dfa.nodes[dfa.root].hasActions(), "All actions on the DFA root should come from regular expressions that match everything.");

        if (!firstNFAWithoutDomainsSeen) {
            // Put all the universal actions on the first DFA.
            addUniversalActionsToDFA(dfa, universalActionsWithoutDomains);
        }

        Vector<DFABytecode> bytecode;
        DFABytecodeCompiler compiler(dfa, bytecode);
        compiler.compile();
        LOG_LARGE_STRUCTURES(bytecode, bytecode.capacity() * sizeof(uint8_t));
        client.writeFiltersWithoutDomainsBytecode(WTF::move(bytecode));

        firstNFAWithoutDomainsSeen = true;
    });
    ASSERT(filtersWithoutDomains.isEmpty());

    if (!firstNFAWithoutDomainsSeen) {
        // Our bytecode interpreter expects to have at least one DFA, so if we haven't seen any
        // create a dummy one and add any universal actions.

        NFA dummyNFA;
        DFA dummyDFA = NFAToDFA::convert(dummyNFA);

        addUniversalActionsToDFA(dummyDFA, universalActionsWithoutDomains);

        Vector<DFABytecode> bytecode;
        DFABytecodeCompiler compiler(dummyDFA, bytecode);
        compiler.compile();
        LOG_LARGE_STRUCTURES(bytecode, bytecode.capacity() * sizeof(uint8_t));
        client.writeFiltersWithoutDomainsBytecode(WTF::move(bytecode));
    }
    LOG_LARGE_STRUCTURES(universalActionsWithoutDomains, universalActionsWithoutDomains.capacity() * sizeof(unsigned));
    universalActionsWithoutDomains.clear();
    
    bool firstNFAWithDomainsSeen = false;
    filtersWithDomains.processNFAs(maxNFASize, [&](NFA&& nfa) {
#if CONTENT_EXTENSIONS_STATE_MACHINE_DEBUGGING
        dataLogF("filtersWithDomains NFA\n");
        nfa.debugPrintDot();
#endif
        LOG_LARGE_STRUCTURES(nfa, nfa.memoryUsed());
        DFA dfa = NFAToDFA::convert(nfa);
#if CONTENT_EXTENSIONS_STATE_MACHINE_DEBUGGING
        dataLogF("filtersWithDomains PRE MINIMIZING DFA\n");
        dfa.debugPrintDot();
#endif
        LOG_LARGE_STRUCTURES(dfa, dfa.memoryUsed());
        // Minimizing this DFA would not be effective because all actions with domains are unique.
#if CONTENT_EXTENSIONS_STATE_MACHINE_DEBUGGING
        dataLogF("filtersWithDomains POST MINIMIZING DFA\n");
        dfa.debugPrintDot();
#endif
        ASSERT_WITH_MESSAGE(!dfa.nodes[dfa.root].hasActions(), "Filters with domains that match everything are not allowed right now.");
        
        if (!firstNFAWithDomainsSeen) {
            // Put all the universal actions on the first DFA.
            addUniversalActionsToDFA(dfa, universalActionsWithDomains);
        }
        
        Vector<DFABytecode> bytecode;
        DFABytecodeCompiler compiler(dfa, bytecode);
        compiler.compile();
        LOG_LARGE_STRUCTURES(bytecode, bytecode.capacity() * sizeof(uint8_t));
        client.writeFiltersWithDomainsBytecode(WTF::move(bytecode));
        
        firstNFAWithDomainsSeen = true;
    });
    ASSERT(filtersWithDomains.isEmpty());
    
    if (!firstNFAWithDomainsSeen) {
        // Our bytecode interpreter expects to have at least one DFA, so if we haven't seen any
        // create a dummy one and add any universal actions.
        
        NFA dummyNFA;
        DFA dummyDFA = NFAToDFA::convert(dummyNFA);
        
        addUniversalActionsToDFA(dummyDFA, universalActionsWithDomains);
        
        Vector<DFABytecode> bytecode;
        DFABytecodeCompiler compiler(dummyDFA, bytecode);
        compiler.compile();
        LOG_LARGE_STRUCTURES(bytecode, bytecode.capacity() * sizeof(uint8_t));
        client.writeFiltersWithDomainsBytecode(WTF::move(bytecode));
    }
    LOG_LARGE_STRUCTURES(universalActionsWithDomains, universalActionsWithDomains.capacity() * sizeof(unsigned));
    universalActionsWithDomains.clear();

    domainFilters.processNFAs(maxNFASize, [&](NFA&& nfa) {
#if CONTENT_EXTENSIONS_STATE_MACHINE_DEBUGGING
        dataLogF("domainFilters NFA\n");
        nfa.debugPrintDot();
#endif
        LOG_LARGE_STRUCTURES(nfa, nfa.memoryUsed());
        DFA dfa = NFAToDFA::convert(nfa);
#if CONTENT_EXTENSIONS_STATE_MACHINE_DEBUGGING
        dataLogF("domainFilters DFA\n");
        dfa.debugPrintDot();
#endif
        LOG_LARGE_STRUCTURES(dfa, dfa.memoryUsed());
        // Minimizing this DFA would not be effective because all actions are unique
        // and because of the tree-like structure of this DFA.
        ASSERT_WITH_MESSAGE(!dfa.nodes[dfa.root].hasActions(), "There should not be any domains that match everything.");
        
        Vector<DFABytecode> bytecode;
        DFABytecodeCompiler compiler(dfa, bytecode);
        compiler.compile();
        LOG_LARGE_STRUCTURES(bytecode, bytecode.capacity() * sizeof(uint8_t));
        client.writeDomainFiltersBytecode(WTF::move(bytecode));
    });
    ASSERT(domainFilters.isEmpty());    
    
#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    double totalNFAToByteCodeBuildTimeEnd = monotonicallyIncreasingTime();
    dataLogF("    Time spent building and compiling the DFAs: %f\n", (totalNFAToByteCodeBuildTimeEnd - totalNFAToByteCodeBuildTimeStart));
#endif

    client.finalize();

    return { };
}

} // namespace ContentExtensions
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
