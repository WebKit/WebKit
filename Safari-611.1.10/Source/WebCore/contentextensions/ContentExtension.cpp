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
#include "ContentExtension.h"

#include "CompiledContentExtension.h"
#include "ContentExtensionsBackend.h"
#include "StyleSheetContents.h"
#include <wtf/text/StringBuilder.h>

#if ENABLE(CONTENT_EXTENSIONS)

namespace WebCore {
namespace ContentExtensions {

Ref<ContentExtension> ContentExtension::create(const String& identifier, Ref<CompiledContentExtension>&& compiledExtension, ShouldCompileCSS shouldCompileCSS)
{
    return adoptRef(*new ContentExtension(identifier, WTFMove(compiledExtension), shouldCompileCSS));
}

ContentExtension::ContentExtension(const String& identifier, Ref<CompiledContentExtension>&& compiledExtension, ShouldCompileCSS shouldCompileCSS)
    : m_identifier(identifier)
    , m_compiledExtension(WTFMove(compiledExtension))
{
    DFABytecodeInterpreter withoutConditions(m_compiledExtension->filtersWithoutConditionsBytecode(), m_compiledExtension->filtersWithoutConditionsBytecodeLength());
    DFABytecodeInterpreter withConditions(m_compiledExtension->filtersWithConditionsBytecode(), m_compiledExtension->filtersWithConditionsBytecodeLength());
    for (uint64_t action : withoutConditions.actionsMatchingEverything()) {
        ASSERT(static_cast<uint32_t>(action) == action);
        m_universalActionsWithoutConditions.append(static_cast<uint32_t>(action));
    }
    for (uint64_t action : withConditions.actionsMatchingEverything()) {
        ASSERT((action & ~IfConditionFlag) == static_cast<uint32_t>(action));
        m_universalActionsWithConditions.append(action);
    }

    if (shouldCompileCSS == ShouldCompileCSS::Yes)
        compileGlobalDisplayNoneStyleSheet();
    m_universalActionsWithoutConditions.shrinkToFit();
    m_universalActionsWithConditions.shrinkToFit();
}

uint32_t ContentExtension::findFirstIgnorePreviousRules() const
{
    auto* actions = m_compiledExtension->actions();
    uint32_t actionsLength = m_compiledExtension->actionsLength();
    uint32_t currentActionIndex = 0;
    while (currentActionIndex < actionsLength) {
        if (Action::deserializeType(actions, actionsLength, currentActionIndex) == ActionType::IgnorePreviousRules)
            return currentActionIndex;
        currentActionIndex += Action::serializedLength(actions, actionsLength, currentActionIndex);
    }
    return std::numeric_limits<uint32_t>::max();
}
    
StyleSheetContents* ContentExtension::globalDisplayNoneStyleSheet()
{
    return m_globalDisplayNoneStyleSheet.get();
}

void ContentExtension::compileGlobalDisplayNoneStyleSheet()
{
    uint32_t firstIgnorePreviousRules = findFirstIgnorePreviousRules();
    
    auto* actions = m_compiledExtension->actions();
    uint32_t actionsLength = m_compiledExtension->actionsLength();

    auto inGlobalDisplayNoneStyleSheet = [&](const uint32_t location)
    {
        return location < firstIgnorePreviousRules && Action::deserializeType(actions, actionsLength, location) == ActionType::CSSDisplayNoneSelector;
    };
    
    StringBuilder css;
    for (uint32_t universalActionLocation : m_universalActionsWithoutConditions) {
        if (inGlobalDisplayNoneStyleSheet(universalActionLocation)) {
            if (!css.isEmpty())
                css.append(',');
            Action action = Action::deserialize(actions, actionsLength, universalActionLocation);
            ASSERT(action.type() == ActionType::CSSDisplayNoneSelector);
            css.append(action.stringArgument());
        }
    }
    if (css.isEmpty())
        return;
    css.append('{');
    css.append(ContentExtensionsBackend::displayNoneCSSRule());
    css.append('}');

    m_globalDisplayNoneStyleSheet = StyleSheetContents::create();
    m_globalDisplayNoneStyleSheet->setIsUserStyleSheet(true);
    if (!m_globalDisplayNoneStyleSheet->parseString(css.toString()))
        m_globalDisplayNoneStyleSheet = nullptr;

    // These actions don't need to be applied individually any more. They will all be applied to every page as a precompiled style sheet.
    m_universalActionsWithoutConditions.removeAllMatching(inGlobalDisplayNoneStyleSheet);
}

void ContentExtension::populateConditionCacheIfNeeded(const URL& topURL)
{
    if (m_cachedTopURL != topURL) {
        DFABytecodeInterpreter interpreter(m_compiledExtension->topURLFiltersBytecode(), m_compiledExtension->topURLFiltersBytecodeLength());
        const uint16_t allLoadTypesAndResourceTypes = LoadTypeMask | ResourceTypeMask;
        String string = m_compiledExtension->conditionsApplyOnlyToDomain() ? topURL.host().toString() : topURL.string();
        auto topURLActions = interpreter.interpret(string.utf8(), allLoadTypesAndResourceTypes);
        
        m_cachedTopURLActions.clear();
        for (uint64_t action : topURLActions)
            m_cachedTopURLActions.add(action);
        for (uint64_t action : interpreter.actionsMatchingEverything())
            m_cachedTopURLActions.add(action);
        
        m_cachedUniversalConditionedActions.clear();
        for (uint64_t action : m_universalActionsWithConditions) {
        ASSERT_WITH_MESSAGE((action & ~IfConditionFlag) == static_cast<uint32_t>(action), "Universal actions with conditions should not have flags.");
            if (!!(action & IfConditionFlag) == m_cachedTopURLActions.contains(action))
                m_cachedUniversalConditionedActions.append(static_cast<uint32_t>(action));
        }
        m_cachedUniversalConditionedActions.shrinkToFit();
        m_cachedTopURL = topURL;
    }
}

const DFABytecodeInterpreter::Actions& ContentExtension::topURLActions(const URL& topURL)
{
    populateConditionCacheIfNeeded(topURL);
    return m_cachedTopURLActions;
}

const Vector<uint32_t>& ContentExtension::universalActionsWithConditions(const URL& topURL)
{
    populateConditionCacheIfNeeded(topURL);
    return m_cachedUniversalConditionedActions;
}
    
} // namespace ContentExtensions
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
