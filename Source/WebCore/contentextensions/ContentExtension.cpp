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

RefPtr<ContentExtension> ContentExtension::create(const String& identifier, Ref<CompiledContentExtension>&& compiledExtension)
{
    return adoptRef(*new ContentExtension(identifier, WTFMove(compiledExtension)));
}

ContentExtension::ContentExtension(const String& identifier, Ref<CompiledContentExtension>&& compiledExtension)
    : m_identifier(identifier)
    , m_compiledExtension(WTFMove(compiledExtension))
{
    DFABytecodeInterpreter withoutDomains(m_compiledExtension->filtersWithoutDomainsBytecode(), m_compiledExtension->filtersWithoutDomainsBytecodeLength());
    DFABytecodeInterpreter withDomains(m_compiledExtension->filtersWithDomainsBytecode(), m_compiledExtension->filtersWithDomainsBytecodeLength());
    for (uint64_t action : withoutDomains.actionsMatchingEverything()) {
        ASSERT(static_cast<uint32_t>(action) == action);
        m_universalActionsWithoutDomains.append(static_cast<uint32_t>(action));
    }
    for (uint64_t action : withDomains.actionsMatchingEverything()) {
        ASSERT((action & ~IfDomainFlag) == static_cast<uint32_t>(action));
        m_universalActionsWithDomains.append(action);
    }
    
    compileGlobalDisplayNoneStyleSheet();
    m_universalActionsWithoutDomains.shrinkToFit();
    m_universalActionsWithDomains.shrinkToFit();
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
    for (uint32_t universalActionLocation : m_universalActionsWithoutDomains) {
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
    css.append("{");
    css.append(ContentExtensionsBackend::displayNoneCSSRule());
    css.append("}");

    m_globalDisplayNoneStyleSheet = StyleSheetContents::create();
    m_globalDisplayNoneStyleSheet->setIsUserStyleSheet(true);
    if (!m_globalDisplayNoneStyleSheet->parseString(css.toString()))
        m_globalDisplayNoneStyleSheet = nullptr;

    // These actions don't need to be applied individually any more. They will all be applied to every page as a precompiled style sheet.
    m_universalActionsWithoutDomains.removeAllMatching(inGlobalDisplayNoneStyleSheet);
}

void ContentExtension::populateDomainCacheIfNeeded(const String& domain)
{
    if (m_cachedDomain != domain) {
        DFABytecodeInterpreter interpreter(m_compiledExtension->domainFiltersBytecode(), m_compiledExtension->domainFiltersBytecodeLength());
        const uint16_t allLoadTypesAndResourceTypes = LoadTypeMask | ResourceTypeMask;
        auto domainActions = interpreter.interpret(domain.utf8(), allLoadTypesAndResourceTypes);
        
        m_cachedDomainActions.clear();
        for (uint64_t action : domainActions)
            m_cachedDomainActions.add(action);
        
        m_cachedUniversalDomainActions.clear();
        for (uint64_t action : m_universalActionsWithDomains) {
            ASSERT_WITH_MESSAGE((action & ~IfDomainFlag) == static_cast<uint32_t>(action), "Universal actions with domains should not have flags.");
            if (!!(action & IfDomainFlag) == m_cachedDomainActions.contains(action))
                m_cachedUniversalDomainActions.append(static_cast<uint32_t>(action));
        }
        m_cachedUniversalDomainActions.shrinkToFit();
        m_cachedDomain = domain;
    }
}

const DFABytecodeInterpreter::Actions& ContentExtension::cachedDomainActions(const String& domain)
{
    populateDomainCacheIfNeeded(domain);
    return m_cachedDomainActions;
}

const Vector<uint32_t>& ContentExtension::universalActionsWithDomains(const String& domain)
{
    populateDomainCacheIfNeeded(domain);
    return m_cachedUniversalDomainActions;
}
    
} // namespace ContentExtensions
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
