/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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

namespace WebCore::ContentExtensions {

Ref<ContentExtension> ContentExtension::create(const String& identifier, Ref<CompiledContentExtension>&& compiledExtension, URL&& extensionBaseURL, ShouldCompileCSS shouldCompileCSS)
{
    return adoptRef(*new ContentExtension(identifier, WTFMove(compiledExtension), WTFMove(extensionBaseURL), shouldCompileCSS));
}

ContentExtension::ContentExtension(const String& identifier, Ref<CompiledContentExtension>&& compiledExtension, URL&& extensionBaseURL, ShouldCompileCSS shouldCompileCSS)
    : m_identifier(identifier)
    , m_compiledExtension(WTFMove(compiledExtension))
    , m_extensionBaseURL(WTFMove(extensionBaseURL))
{
    DFABytecodeInterpreter interpreter(m_compiledExtension->urlFiltersBytecode());
    for (uint64_t action : interpreter.actionsMatchingEverything())
        m_universalActions.append(action);

    if (shouldCompileCSS == ShouldCompileCSS::Yes)
        compileGlobalDisplayNoneStyleSheet();
    m_universalActions.shrinkToFit();
}

uint32_t ContentExtension::findFirstIgnorePreviousRules() const
{
    auto serializedActions = m_compiledExtension->serializedActions();
    uint32_t currentActionIndex = 0;
    while (currentActionIndex < serializedActions.size()) {
        if (serializedActions[currentActionIndex] == WTF::alternativeIndexV<IgnorePreviousRulesAction, ActionData>)
            return currentActionIndex;
        currentActionIndex += DeserializedAction::serializedLength(serializedActions, currentActionIndex);
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
    
    auto serializedActions = m_compiledExtension->serializedActions();

    auto inGlobalDisplayNoneStyleSheet = [&](const uint32_t location) {
        RELEASE_ASSERT(location < serializedActions.size());
        return location < firstIgnorePreviousRules && serializedActions[location] == WTF::alternativeIndexV<CSSDisplayNoneSelectorAction, ActionData>;
    };
    
    StringBuilder css;
    for (uint64_t universalActionLocation : m_universalActions) {
        if (inGlobalDisplayNoneStyleSheet(universalActionLocation)) {
            if (!css.isEmpty())
                css.append(',');
            auto action = DeserializedAction::deserialize(serializedActions, universalActionLocation);
            ASSERT(std::holds_alternative<CSSDisplayNoneSelectorAction>(action.data()));
            if (auto* actionData = std::get_if<CSSDisplayNoneSelectorAction>(&action.data()))
                css.append(actionData->string);
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
    m_universalActions.removeAllMatching(inGlobalDisplayNoneStyleSheet);
}

void ContentExtension::populateTopURLActionCacheIfNeeded(const URL& topURL) const
{
    if (m_cachedTopURL == topURL)
        return;

    DFABytecodeInterpreter interpreter(m_compiledExtension->topURLFiltersBytecode());
    auto topURLActions = interpreter.interpret(topURL.string(), AllResourceFlags);

    m_cachedTopURLActions.clear();
    for (uint64_t action : topURLActions)
        m_cachedTopURLActions.add(action);
    for (uint64_t action : interpreter.actionsMatchingEverything())
        m_cachedTopURLActions.add(action);

    m_cachedTopURL = topURL;
}

void ContentExtension::populateFrameURLActionCacheIfNeeded(const URL& frameURL) const
{
    if (m_cachedFrameURL == frameURL)
        return;

    DFABytecodeInterpreter interpreter(m_compiledExtension->frameURLFiltersBytecode());
    auto frameURLActions = interpreter.interpret(frameURL.string(), AllResourceFlags);

    m_cachedFrameURLActions.clear();
    for (uint64_t action : frameURLActions)
        m_cachedFrameURLActions.add(action);
    for (uint64_t action : interpreter.actionsMatchingEverything())
        m_cachedFrameURLActions.add(action);

    m_cachedFrameURL = frameURL;
}

const DFABytecodeInterpreter::Actions& ContentExtension::topURLActions(const URL& topURL) const
{
    populateTopURLActionCacheIfNeeded(topURL);
    return m_cachedTopURLActions;
}

const DFABytecodeInterpreter::Actions& ContentExtension::frameURLActions(const URL& frameURL) const
{
    populateFrameURLActionCacheIfNeeded(frameURL);
    return m_cachedFrameURLActions;
}

} // namespace WebCore::ContentExtensions

#endif // ENABLE(CONTENT_EXTENSIONS)
