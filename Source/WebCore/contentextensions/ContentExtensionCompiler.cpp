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
#include "DFACombiner.h"
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
    actions.grow(actions.size() + sizeof(unsigned));
    *reinterpret_cast<unsigned*>(&actions[actions.size() - sizeof(unsigned)]) = selectorLength;
    bool wideCharacters = !selector.is8Bit();
    actions.append(wideCharacters);
    // Append Selector.
    if (wideCharacters) {
        unsigned startIndex = actions.size();
        actions.grow(actions.size() + sizeof(UChar) * selectorLength);
        for (unsigned i = 0; i < selectorLength; ++i)
            *reinterpret_cast<UChar*>(&actions[startIndex + i * sizeof(UChar)]) = selector[i];
    } else {
        for (unsigned i = 0; i < selectorLength; ++i)
            actions.append(selector[i]);
    }
}

struct PendingDisplayNoneActions {
    Vector<String> selectors;
    Vector<unsigned> clientLocations;
};
typedef HashMap<Trigger, PendingDisplayNoneActions, TriggerHash, TriggerHashTraits> PendingDisplayNoneActionsMap;

static void resolvePendingDisplayNoneActions(Vector<SerializedActionByte>& actions, Vector<unsigned>& actionLocations, PendingDisplayNoneActionsMap& pendingDisplayNoneActionsMap)
{
    for (auto& slot : pendingDisplayNoneActionsMap) {
        PendingDisplayNoneActions& pendingActions = slot.value;

        StringBuilder combinedSelectors;
        for (unsigned i = 0; i < pendingActions.selectors.size(); ++i) {
            if (i)
                combinedSelectors.append(',');
            combinedSelectors.append(pendingActions.selectors[i]);
        }

        unsigned actionLocation = actions.size();
        serializeSelector(actions, combinedSelectors.toString());
        for (unsigned clientLocation : pendingActions.clientLocations)
            actionLocations[clientLocation] = actionLocation;
    }
    pendingDisplayNoneActionsMap.clear();
}

static Vector<unsigned> serializeActions(const Vector<ContentExtensionRule>& ruleList, Vector<SerializedActionByte>& actions)
{
    ASSERT(!actions.size());

    Vector<unsigned> actionLocations;

    // Order only matters because of IgnorePreviousRules. All other identical actions can be combined between each IgnorePreviousRules
    // and CSSDisplayNone strings can be combined if their triggers are identical.
    typedef HashMap<uint32_t, uint32_t, DefaultHash<uint32_t>::Hash, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> ActionMap;
    ActionMap blockLoadActionsMap;
    ActionMap blockCookiesActionsMap;
    PendingDisplayNoneActionsMap cssDisplayNoneActionsMap;
    ActionMap ignorePreviousRuleActionsMap;
    ActionMap makeHTTPSActionsMap;

    for (unsigned ruleIndex = 0; ruleIndex < ruleList.size(); ++ruleIndex) {
        const ContentExtensionRule& rule = ruleList[ruleIndex];
        ActionType actionType = rule.action().type();

        if (actionType == ActionType::IgnorePreviousRules) {
            resolvePendingDisplayNoneActions(actions, actionLocations, cssDisplayNoneActionsMap);

            blockLoadActionsMap.clear();
            blockCookiesActionsMap.clear();
            cssDisplayNoneActionsMap.clear();
            makeHTTPSActionsMap.clear();
        } else
            ignorePreviousRuleActionsMap.clear();

        // Anything with condition is just pushed.
        // We could try to merge conditions but that case is not common in practice.
        if (!rule.trigger().conditions.isEmpty()) {
            actionLocations.append(actions.size());

            if (actionType == ActionType::CSSDisplayNoneSelector)
                serializeSelector(actions, rule.action().stringArgument());
            else
                actions.append(static_cast<SerializedActionByte>(actionType));
            continue;
        }

        ResourceFlags flags = rule.trigger().flags;
        unsigned actionLocation = std::numeric_limits<unsigned>::max();
        
        auto findOrMakeActionLocation = [&] (ActionMap& map) 
        {
            const auto existingAction = map.find(flags);
            if (existingAction == map.end()) {
                actionLocation = actions.size();
                actions.append(static_cast<SerializedActionByte>(actionType));
                map.set(flags, actionLocation);
            } else
                actionLocation = existingAction->value;
        };

        switch (actionType) {
        case ActionType::CSSDisplayNoneStyleSheet:
        case ActionType::InvalidAction:
            RELEASE_ASSERT_NOT_REACHED();

        case ActionType::CSSDisplayNoneSelector: {
            const auto addResult = cssDisplayNoneActionsMap.add(rule.trigger(), PendingDisplayNoneActions());
            PendingDisplayNoneActions& pendingDisplayNoneActions = addResult.iterator->value;
            pendingDisplayNoneActions.selectors.append(rule.action().stringArgument());
            pendingDisplayNoneActions.clientLocations.append(actionLocations.size());

            actionLocation = std::numeric_limits<unsigned>::max();
            break;
        }
        case ActionType::IgnorePreviousRules:
            findOrMakeActionLocation(ignorePreviousRuleActionsMap);
            break;
        case ActionType::BlockLoad:
            findOrMakeActionLocation(blockLoadActionsMap);
            break;
        case ActionType::BlockCookies:
            findOrMakeActionLocation(blockCookiesActionsMap);
            break;
        case ActionType::MakeHTTPS:
            findOrMakeActionLocation(makeHTTPSActionsMap);
            break;
        }

        actionLocations.append(actionLocation);
    }
    resolvePendingDisplayNoneActions(actions, actionLocations, cssDisplayNoneActionsMap);
    return actionLocations;
}

typedef HashSet<uint64_t, DefaultHash<uint64_t>::Hash, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>> UniversalActionSet;

static void addUniversalActionsToDFA(DFA& dfa, UniversalActionSet&& universalActions)
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

template<typename Functor>
static void compileToBytecode(CombinedURLFilters&& filters, UniversalActionSet&& universalActions, Functor writeBytecodeToClient)
{
    // Smaller maxNFASizes risk high compiling and interpreting times from having too many DFAs,
    // larger maxNFASizes use too much memory when compiling.
    const unsigned maxNFASize = 75000;

    bool firstNFASeen = false;

    auto lowerDFAToBytecode = [&](DFA&& dfa)
    {
#if CONTENT_EXTENSIONS_STATE_MACHINE_DEBUGGING
        dataLogF("DFA\n");
        dfa.debugPrintDot();
#endif
        ASSERT_WITH_MESSAGE(!dfa.nodes[dfa.root].hasActions(), "All actions on the DFA root should come from regular expressions that match everything.");

        if (!firstNFASeen) {
            // Put all the universal actions on the first DFA.
            addUniversalActionsToDFA(dfa, WTFMove(universalActions));
        }

        Vector<DFABytecode> bytecode;
        DFABytecodeCompiler compiler(dfa, bytecode);
        compiler.compile();
        LOG_LARGE_STRUCTURES(bytecode, bytecode.capacity() * sizeof(uint8_t));
        writeBytecodeToClient(WTFMove(bytecode));
        firstNFASeen = true;
    };

    const unsigned smallDFASize = 100;
    DFACombiner smallDFACombiner;
    filters.processNFAs(maxNFASize, [&](NFA&& nfa) {
#if CONTENT_EXTENSIONS_STATE_MACHINE_DEBUGGING
        dataLogF("NFA\n");
        nfa.debugPrintDot();
#endif
        LOG_LARGE_STRUCTURES(nfa, nfa.memoryUsed());
        DFA dfa = NFAToDFA::convert(nfa);
        LOG_LARGE_STRUCTURES(dfa, dfa.memoryUsed());

        if (dfa.graphSize() < smallDFASize)
            smallDFACombiner.addDFA(WTFMove(dfa));
        else {
            dfa.minimize();
            lowerDFAToBytecode(WTFMove(dfa));
        }
    });

    smallDFACombiner.combineDFAs(smallDFASize, [&](DFA&& dfa) {
        LOG_LARGE_STRUCTURES(dfa, dfa.memoryUsed());
        lowerDFAToBytecode(WTFMove(dfa));
    });

    ASSERT(filters.isEmpty());

    if (!firstNFASeen) {
        // Our bytecode interpreter expects to have at least one DFA, so if we haven't seen any
        // create a dummy one and add any universal actions.

        DFA dummyDFA = DFA::empty();
        addUniversalActionsToDFA(dummyDFA, WTFMove(universalActions));

        Vector<DFABytecode> bytecode;
        DFABytecodeCompiler compiler(dummyDFA, bytecode);
        compiler.compile();
        LOG_LARGE_STRUCTURES(bytecode, bytecode.capacity() * sizeof(uint8_t));
        writeBytecodeToClient(WTFMove(bytecode));
    }
    LOG_LARGE_STRUCTURES(universalActions, universalActions.capacity() * sizeof(unsigned));
}

std::error_code compileRuleList(ContentExtensionCompilationClient& client, String&& ruleJSON)
{
    auto ruleList = parseRuleList(WTFMove(ruleJSON));
    if (!ruleList.hasValue())
        return ruleList.error();
    Vector<ContentExtensionRule> parsedRuleList = WTFMove(ruleList.value());

    bool domainConditionSeen = false;
    bool topURLConditionSeen = false;
    for (const auto& rule : parsedRuleList) {
        switch (rule.trigger().conditionType) {
        case Trigger::ConditionType::None:
            break;
        case Trigger::ConditionType::IfDomain:
        case Trigger::ConditionType::UnlessDomain:
            domainConditionSeen = true;
            break;
        case Trigger::ConditionType::IfTopURL:
        case Trigger::ConditionType::UnlessTopURL:
            topURLConditionSeen = true;
            break;
        }
    }
    if (topURLConditionSeen && domainConditionSeen)
        return ContentExtensionError::JSONTopURLAndDomainConditions;

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    double patternPartitioningStart = monotonicallyIncreasingTime();
#endif
    
    client.writeSource(ruleJSON);

    Vector<SerializedActionByte> actions;
    Vector<unsigned> actionLocations = serializeActions(parsedRuleList, actions);
    LOG_LARGE_STRUCTURES(actions, actions.capacity() * sizeof(SerializedActionByte));
    client.writeActions(WTFMove(actions), domainConditionSeen);

    UniversalActionSet universalActionsWithoutConditions;
    UniversalActionSet universalActionsWithConditions;
    UniversalActionSet universalTopURLActions;

    // FIXME: These don't all need to be in memory at the same time.
    CombinedURLFilters filtersWithoutConditions;
    CombinedURLFilters filtersWithConditions;
    CombinedURLFilters topURLFilters;
    URLFilterParser filtersWithoutConditionParser(filtersWithoutConditions);
    URLFilterParser filtersWithConditionParser(filtersWithConditions);
    URLFilterParser topURLFilterParser(topURLFilters);
    
    for (unsigned ruleIndex = 0; ruleIndex < parsedRuleList.size(); ++ruleIndex) {
        const ContentExtensionRule& contentExtensionRule = parsedRuleList[ruleIndex];
        const Trigger& trigger = contentExtensionRule.trigger();
        ASSERT(trigger.urlFilter.length());

        // High bits are used for flags. This should match how they are used in DFABytecodeCompiler::compileNode.
        ASSERT(!trigger.flags || ActionFlagMask & (static_cast<uint64_t>(trigger.flags) << 32));
        ASSERT(!(~ActionFlagMask & (static_cast<uint64_t>(trigger.flags) << 32)));
        uint64_t actionLocationAndFlags = (static_cast<uint64_t>(trigger.flags) << 32) | static_cast<uint64_t>(actionLocations[ruleIndex]);
        URLFilterParser::ParseStatus status = URLFilterParser::Ok;
        if (trigger.conditions.isEmpty()) {
            ASSERT(trigger.conditionType == Trigger::ConditionType::None);
            status = filtersWithoutConditionParser.addPattern(trigger.urlFilter, trigger.urlFilterIsCaseSensitive, actionLocationAndFlags);
            if (status == URLFilterParser::MatchesEverything) {
                universalActionsWithoutConditions.add(actionLocationAndFlags);
                status = URLFilterParser::Ok;
            }
            if (status != URLFilterParser::Ok) {
                dataLogF("Error while parsing %s: %s\n", trigger.urlFilter.utf8().data(), URLFilterParser::statusString(status).utf8().data());
                return ContentExtensionError::JSONInvalidRegex;
            }
        } else {
            switch (trigger.conditionType) {
            case Trigger::ConditionType::IfDomain:
            case Trigger::ConditionType::IfTopURL:
                actionLocationAndFlags |= IfConditionFlag;
                break;
            case Trigger::ConditionType::None:
            case Trigger::ConditionType::UnlessDomain:
            case Trigger::ConditionType::UnlessTopURL:
                ASSERT(!(actionLocationAndFlags & IfConditionFlag));
                break;
            }
            
            status = filtersWithConditionParser.addPattern(trigger.urlFilter, trigger.urlFilterIsCaseSensitive, actionLocationAndFlags);
            if (status == URLFilterParser::MatchesEverything) {
                universalActionsWithConditions.add(actionLocationAndFlags);
                status = URLFilterParser::Ok;
            }
            if (status != URLFilterParser::Ok) {
                dataLogF("Error while parsing %s: %s\n", trigger.urlFilter.utf8().data(), URLFilterParser::statusString(status).utf8().data());
                return ContentExtensionError::JSONInvalidRegex;
            }
            for (const String& condition : trigger.conditions) {
                if (domainConditionSeen) {
                    ASSERT(!topURLConditionSeen);
                    topURLFilters.addDomain(actionLocationAndFlags, condition);
                } else {
                    ASSERT(topURLConditionSeen);
                    status = topURLFilterParser.addPattern(condition, trigger.topURLConditionIsCaseSensitive, actionLocationAndFlags);
                    if (status == URLFilterParser::MatchesEverything) {
                        universalTopURLActions.add(actionLocationAndFlags);
                        status = URLFilterParser::Ok;
                    }
                    if (status != URLFilterParser::Ok) {
                        dataLogF("Error while parsing %s: %s\n", condition.utf8().data(), URLFilterParser::statusString(status).utf8().data());
                        return ContentExtensionError::JSONInvalidRegex;
                    }
                }
            }
        }
        ASSERT(status == URLFilterParser::Ok);
    }
    LOG_LARGE_STRUCTURES(parsedRuleList, parsedRuleList.capacity() * sizeof(ContentExtensionRule)); // Doesn't include strings.
    LOG_LARGE_STRUCTURES(actionLocations, actionLocations.capacity() * sizeof(unsigned));
    parsedRuleList.clear();
    actionLocations.clear();

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    double patternPartitioningEnd = monotonicallyIncreasingTime();
    dataLogF("    Time spent partitioning the rules into groups: %f\n", (patternPartitioningEnd - patternPartitioningStart));
#endif

    LOG_LARGE_STRUCTURES(filtersWithoutConditions, filtersWithoutConditions.memoryUsed());
    LOG_LARGE_STRUCTURES(filtersWithConditions, filtersWithConditions.memoryUsed());
    LOG_LARGE_STRUCTURES(topURLFilters, topURLFilters.memoryUsed());

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    double totalNFAToByteCodeBuildTimeStart = monotonicallyIncreasingTime();
#endif

    compileToBytecode(WTFMove(filtersWithoutConditions), WTFMove(universalActionsWithoutConditions), [&](Vector<DFABytecode>&& bytecode) {
        client.writeFiltersWithoutConditionsBytecode(WTFMove(bytecode));
    });
    compileToBytecode(WTFMove(filtersWithConditions), WTFMove(universalActionsWithConditions), [&](Vector<DFABytecode>&& bytecode) {
        client.writeFiltersWithConditionsBytecode(WTFMove(bytecode));
    });
    compileToBytecode(WTFMove(topURLFilters), WTFMove(universalTopURLActions), [&](Vector<DFABytecode>&& bytecode) {
        client.writeTopURLFiltersBytecode(WTFMove(bytecode));
    });
    
#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    double totalNFAToByteCodeBuildTimeEnd = monotonicallyIncreasingTime();
    dataLogF("    Time spent building and compiling the DFAs: %f\n", (totalNFAToByteCodeBuildTimeEnd - totalNFAToByteCodeBuildTimeStart));

    dataLogF("    Number of machines without condition filters: %d (total bytecode size = %d)\n", machinesWithoutConditionsCount, totalBytecodeSizeForMachinesWithoutConditions);
    dataLogF("    Number of machines with condition filters: %d (total bytecode size = %d)\n", machinesWithConditionsCount, totalBytecodeSizeForMachinesWithConditions);
#endif

    client.finalize();

    return { };
}

} // namespace ContentExtensions
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
