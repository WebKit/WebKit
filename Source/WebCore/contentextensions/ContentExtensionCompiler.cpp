/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
#include <wtf/CrossThreadCopier.h>
#include <wtf/DataLog.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore::ContentExtensions {

// css-display-none combining is special because we combine the string arguments with commas because we know they are css selectors.
struct PendingDisplayNoneActions {
    StringBuilder combinedSelectors;
    Vector<uint32_t> clientLocations;
};

using PendingDisplayNoneActionsMap = HashMap<Trigger, PendingDisplayNoneActions, TriggerHash, TriggerHashTraits>;

static void resolvePendingDisplayNoneActions(Vector<SerializedActionByte>& actions, Vector<uint32_t>& actionLocations, PendingDisplayNoneActionsMap& map)
{
    for (auto& pendingDisplayNoneActions : map.values()) {
        uint32_t actionLocation = actions.size();
        actions.append(WTF::alternativeIndexV<CSSDisplayNoneSelectorAction, ActionData>);
        serializeString(actions, pendingDisplayNoneActions.combinedSelectors.toString());
        for (uint32_t clientLocation : pendingDisplayNoneActions.clientLocations)
            actionLocations[clientLocation] = actionLocation;
    }
    map.clear();
}

static Vector<unsigned> serializeActions(const Vector<ContentExtensionRule>& ruleList, Vector<SerializedActionByte>& actions)
{
    ASSERT(!actions.size());

    Vector<uint32_t> actionLocations;

    using ActionLocation = uint32_t;
    using ActionMap = HashMap<ResourceFlags, ActionLocation, DefaultHash<ResourceFlags>, WTF::UnsignedWithZeroKeyHashTraits<ResourceFlags>>;
    using StringActionMap = HashMap<std::pair<String, ResourceFlags>, ActionLocation, DefaultHash<std::pair<String, ResourceFlags>>, PairHashTraits<HashTraits<String>, WTF::UnsignedWithZeroKeyHashTraits<ResourceFlags>>>;
    using RedirectActionMap = HashMap<std::pair<RedirectAction, ResourceFlags>, ActionLocation, DefaultHash<std::pair<RedirectAction, ResourceFlags>>, PairHashTraits<HashTraits<RedirectAction>, WTF::UnsignedWithZeroKeyHashTraits<ResourceFlags>>>;
    using ModifyHeadersActionMap = HashMap<std::pair<ModifyHeadersAction, ResourceFlags>, ActionLocation, DefaultHash<std::pair<ModifyHeadersAction, ResourceFlags>>, PairHashTraits<HashTraits<ModifyHeadersAction>, WTF::UnsignedWithZeroKeyHashTraits<ResourceFlags>>>;
    ActionMap blockLoadActionsMap;
    ActionMap blockCookiesActionsMap;
    PendingDisplayNoneActionsMap cssDisplayNoneActionsMap;
    ActionMap ignorePreviousRuleActionsMap;
    ActionMap makeHTTPSActionsMap;
    StringActionMap notifyActionsMap;
    RedirectActionMap redirectActionMap;
    ModifyHeadersActionMap modifyHeadersActionMap;

    for (auto& rule : ruleList) {
        auto& actionData = rule.action().data();
        if (std::holds_alternative<IgnorePreviousRulesAction>(actionData)) {
            resolvePendingDisplayNoneActions(actions, actionLocations, cssDisplayNoneActionsMap);

            blockLoadActionsMap.clear();
            blockCookiesActionsMap.clear();
            cssDisplayNoneActionsMap.clear();
            makeHTTPSActionsMap.clear();
            notifyActionsMap.clear();
        } else
            ignorePreviousRuleActionsMap.clear();

        // Anything with condition is just pushed.
        // We could try to merge conditions but that case is not common in practice.
        if (!rule.trigger().conditions.isEmpty()) {
            actionLocations.append(actions.size());

            actions.append(actionData.index());
            std::visit(WTF::makeVisitor([&](const auto& member) {
                member.serialize(actions);
            }), actionData);
            continue;
        }

        ResourceFlags flags = rule.trigger().flags;

        auto findOrMakeActionLocation = [&] (ActionMap& map) {
            return map.ensure(flags, [&] {
                auto newActionLocation = actions.size();
                actions.append(actionData.index());
                return newActionLocation;
            }).iterator->value;
        };

        auto findOrMakeNotifyActionLocation = [&] (auto& map, const auto& action) {
            return map.ensure({ action.string, flags }, [&] {
                auto newActionLocation = actions.size();
                actions.append(actionData.index());
                action.serialize(actions);
                return newActionLocation;
            }).iterator->value;
        };

        auto findOrMakeOtherActionLocation = [&] (auto& map, const auto& action) {
            return map.ensure({ action, flags }, [&] {
                auto newActionLocation = actions.size();
                actions.append(actionData.index());
                action.serialize(actions);
                return newActionLocation;
            }).iterator->value;
        };

        auto actionLocation = std::visit(WTF::makeVisitor([&] (const CSSDisplayNoneSelectorAction& actionData) {
            const auto addResult = cssDisplayNoneActionsMap.add(rule.trigger(), PendingDisplayNoneActions());
            auto& pendingStringActions = addResult.iterator->value;
            if (!pendingStringActions.combinedSelectors.isEmpty())
                pendingStringActions.combinedSelectors.append(',');
            pendingStringActions.combinedSelectors.append(actionData.string);
            pendingStringActions.clientLocations.append(actionLocations.size());

            // resolvePendingDisplayNoneActions will fill this in later.
            return std::numeric_limits<ActionLocation>::max();
        }, [&] (const IgnorePreviousRulesAction&) {
            return findOrMakeActionLocation(ignorePreviousRuleActionsMap);
        }, [&] (const BlockLoadAction&) {
            return findOrMakeActionLocation(blockLoadActionsMap);
        }, [&] (const BlockCookiesAction&) {
            return findOrMakeActionLocation(blockCookiesActionsMap);
        }, [&] (const MakeHTTPSAction&) {
            return findOrMakeActionLocation(makeHTTPSActionsMap);
        }, [&] (const NotifyAction& actionData) {
            return findOrMakeNotifyActionLocation(notifyActionsMap, actionData);
        }, [&] (const ModifyHeadersAction& action) {
            return findOrMakeOtherActionLocation(modifyHeadersActionMap, action);
        }, [&] (const RedirectAction& action) {
            return findOrMakeOtherActionLocation(redirectActionMap, action);
        }), actionData);
        actionLocations.append(actionLocation);
    }
    resolvePendingDisplayNoneActions(actions, actionLocations, cssDisplayNoneActionsMap);
    return actionLocations;
}

using UniversalActionSet = HashSet<uint64_t, DefaultHash<uint64_t>, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>>;

static void addUniversalActionsToDFA(DFA& dfa, UniversalActionSet&& universalActions)
{
    if (universalActions.isEmpty())
        return;

    DFANode& root = dfa.nodes[dfa.root];
    ASSERT(!root.actionsLength());
    unsigned actionsStart = dfa.actions.size();
    dfa.actions.reserveCapacity(dfa.actions.size() + universalActions.size());
    dfa.actions.appendRange(universalActions.begin(), universalActions.end());
    unsigned actionsEnd = dfa.actions.size();

    unsigned actionsLength = actionsEnd - actionsStart;
    RELEASE_ASSERT_WITH_MESSAGE(actionsLength < std::numeric_limits<uint16_t>::max(), "Too many uncombined actions that match everything");
    root.setActions(actionsStart, static_cast<uint16_t>(actionsLength));
}

template<typename Functor>
static bool compileToBytecode(CombinedURLFilters&& filters, UniversalActionSet&& universalActions, Functor writeBytecodeToClient)
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
    bool processedSuccessfully = filters.processNFAs(maxNFASize, [&](NFA&& nfa) {
#if CONTENT_EXTENSIONS_STATE_MACHINE_DEBUGGING
        dataLogF("NFA\n");
        nfa.debugPrintDot();
#endif
        LOG_LARGE_STRUCTURES(nfa, nfa.memoryUsed());
        auto dfa = NFAToDFA::convert(WTFMove(nfa));
        if (!dfa)
            return false;
        LOG_LARGE_STRUCTURES(*dfa, dfa->memoryUsed());

        if (dfa->graphSize() < smallDFASize)
            smallDFACombiner.addDFA(WTFMove(*dfa));
        else {
            dfa->minimize();
            lowerDFAToBytecode(WTFMove(*dfa));
        }
        return true;
    });
    if (!processedSuccessfully)
        return false;

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
    return true;
}

std::error_code compileRuleList(ContentExtensionCompilationClient& client, String&& ruleJSON, Vector<ContentExtensionRule>&& parsedRuleList)
{
#if ASSERT_ENABLED
    callOnMainThread([ruleJSON = ruleJSON.isolatedCopy(), parsedRuleList = crossThreadCopy(parsedRuleList)] {
        ASSERT(parseRuleList(ruleJSON).value() == parsedRuleList);
    });
#endif

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    MonotonicTime patternPartitioningStart = MonotonicTime::now();
#endif
    
    client.writeSource(std::exchange(ruleJSON, String()));

    Vector<SerializedActionByte> actions;
    Vector<unsigned> actionLocations = serializeActions(parsedRuleList, actions);
    LOG_LARGE_STRUCTURES(actions, actions.capacity() * sizeof(SerializedActionByte));
    client.writeActions(WTFMove(actions));

    // FIXME: These don't all need to be in memory at the same time.
    CombinedURLFilters urlFilters;
    UniversalActionSet universalActions;
    URLFilterParser urlFilterParser(urlFilters);

    CombinedURLFilters topURLFilters;
    UniversalActionSet topURLUniversalActions;
    URLFilterParser topURLFilterParser(topURLFilters);

    CombinedURLFilters frameURLFilters;
    UniversalActionSet frameURLUniversalActions;
    URLFilterParser frameURLFilterParser(frameURLFilters);

    for (unsigned ruleIndex = 0; ruleIndex < parsedRuleList.size(); ++ruleIndex) {
        const ContentExtensionRule& contentExtensionRule = parsedRuleList[ruleIndex];
        const Trigger& trigger = contentExtensionRule.trigger();
        ASSERT(trigger.urlFilter.length());

        // High bits are used for flags. This should match how they are used in DFABytecodeCompiler::compileNode.
        ASSERT(!trigger.flags || ActionFlagMask & (static_cast<uint64_t>(trigger.flags) << 32));
        ASSERT(!(~ActionFlagMask & (static_cast<uint64_t>(trigger.flags) << 32)));
        uint64_t actionLocationAndFlags = (static_cast<uint64_t>(trigger.flags) << 32) | static_cast<uint64_t>(actionLocations[ruleIndex]);
        URLFilterParser::ParseStatus status = URLFilterParser::Ok;
        status = urlFilterParser.addPattern(trigger.urlFilter, trigger.urlFilterIsCaseSensitive, actionLocationAndFlags);
        if (status == URLFilterParser::MatchesEverything) {
            universalActions.add(actionLocationAndFlags);
            status = URLFilterParser::Ok;
        }
        if (status != URLFilterParser::Ok) {
            dataLogF("Error while parsing %s: %s\n", trigger.urlFilter.utf8().data(), URLFilterParser::statusString(status).characters());
            return ContentExtensionError::JSONInvalidRegex;
        }

        for (const String& condition : trigger.conditions) {
            switch (static_cast<ActionCondition>(trigger.flags & ActionConditionMask)) {
            case ActionCondition::None:
                ASSERT_NOT_REACHED();
                break;
            case ActionCondition::IfTopURL:
            case ActionCondition::UnlessTopURL:
                status = topURLFilterParser.addPattern(condition, trigger.topURLFilterIsCaseSensitive, actionLocationAndFlags);
                if (status == URLFilterParser::MatchesEverything) {
                    topURLUniversalActions.add(actionLocationAndFlags);
                    status = URLFilterParser::Ok;
                }
                if (status != URLFilterParser::Ok) {
                    dataLogF("Error while parsing %s: %s\n", condition.utf8().data(), URLFilterParser::statusString(status).characters());
                    return ContentExtensionError::JSONInvalidRegex;
                }
                break;
            case ActionCondition::IfFrameURL:
                status = frameURLFilterParser.addPattern(condition, trigger.frameURLFilterIsCaseSensitive, actionLocationAndFlags);
                if (status == URLFilterParser::MatchesEverything) {
                    frameURLUniversalActions.add(actionLocationAndFlags);
                    status = URLFilterParser::Ok;
                }
                if (status != URLFilterParser::Ok) {
                    dataLogF("Error while parsing %s: %s\n", condition.utf8().data(), URLFilterParser::statusString(status).characters());
                    return ContentExtensionError::JSONInvalidRegex;
                }
                break;
            }
        }
        ASSERT(status == URLFilterParser::Ok);
    }
    LOG_LARGE_STRUCTURES(parsedRuleList, parsedRuleList.capacity() * sizeof(ContentExtensionRule)); // Doesn't include strings.
    LOG_LARGE_STRUCTURES(actionLocations, actionLocations.capacity() * sizeof(unsigned));
    parsedRuleList.clear();
    actionLocations.clear();

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    MonotonicTime patternPartitioningEnd = MonotonicTime::now();
    dataLogF("    Time spent partitioning the rules into groups: %f\n", (patternPartitioningEnd - patternPartitioningStart).seconds());
#endif

    LOG_LARGE_STRUCTURES(filtersWithoutConditions, filtersWithoutConditions.memoryUsed());
    LOG_LARGE_STRUCTURES(filtersWithConditions, filtersWithConditions.memoryUsed());
    LOG_LARGE_STRUCTURES(topURLFilters, topURLFilters.memoryUsed());
    LOG_LARGE_STRUCTURES(frameURLFilters, frameURLFilters.memoryUsed());

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    MonotonicTime totalNFAToByteCodeBuildTimeStart = MonotonicTime::now();
#endif

    bool success = compileToBytecode(WTFMove(urlFilters), WTFMove(universalActions), [&](Vector<DFABytecode>&& bytecode) {
        client.writeURLFiltersBytecode(WTFMove(bytecode));
    });
    if (!success)
        return ContentExtensionError::ErrorWritingSerializedNFA;
    success = compileToBytecode(WTFMove(topURLFilters), WTFMove(topURLUniversalActions), [&](Vector<DFABytecode>&& bytecode) {
        client.writeTopURLFiltersBytecode(WTFMove(bytecode));
    });
    if (!success)
        return ContentExtensionError::ErrorWritingSerializedNFA;
    success = compileToBytecode(WTFMove(frameURLFilters), WTFMove(frameURLUniversalActions), [&](Vector<DFABytecode>&& bytecode) {
        client.writeFrameURLFiltersBytecode(WTFMove(bytecode));
    });
    if (!success)
        return ContentExtensionError::ErrorWritingSerializedNFA;

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    MonotonicTime totalNFAToByteCodeBuildTimeEnd = MonotonicTime::now();
    dataLogF("    Time spent building and compiling the DFAs: %f\n", (totalNFAToByteCodeBuildTimeEnd - totalNFAToByteCodeBuildTimeStart).seconds());
#endif

    client.finalize();

    return { };
}

} // namespace WebCore::ContentExtensions

#endif // ENABLE(CONTENT_EXTENSIONS)
