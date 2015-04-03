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

#include "CompiledContentExtension.h"
#include "ContentExtension.h"
#include "DFABytecodeInterpreter.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "MainFrame.h"
#include "ResourceLoadInfo.h"
#include "URL.h"
#include "UserContentController.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/CString.h>

namespace WebCore {

namespace ContentExtensions {
    
void ContentExtensionsBackend::addContentExtension(const String& identifier, RefPtr<CompiledContentExtension> compiledContentExtension)
{
    ASSERT(!identifier.isEmpty());
    if (identifier.isEmpty())
        return;

    if (!compiledContentExtension) {
        removeContentExtension(identifier);
        return;
    }

    RefPtr<ContentExtension> extension = ContentExtension::create(identifier, adoptRef(*compiledContentExtension.leakRef()));
    m_contentExtensions.set(identifier, WTF::move(extension));
}

void ContentExtensionsBackend::removeContentExtension(const String& identifier)
{
    m_contentExtensions.remove(identifier);
}

void ContentExtensionsBackend::removeAllContentExtensions()
{
    m_contentExtensions.clear();
}

Vector<Action> ContentExtensionsBackend::actionsForResourceLoad(const ResourceLoadInfo& resourceLoadInfo) const
{
    const String& urlString = resourceLoadInfo.resourceURL.string();
    ASSERT_WITH_MESSAGE(urlString.containsOnlyASCII(), "A decoded URL should only contain ASCII characters. The matching algorithm assumes the input is ASCII.");
    const CString& urlCString = urlString.utf8();

    Vector<Action> finalActions;
    ResourceFlags flags = resourceLoadInfo.getResourceFlags();
    for (auto& contentExtension : m_contentExtensions.values()) {
        RELEASE_ASSERT(contentExtension);
        const CompiledContentExtension& compiledExtension = contentExtension->compiledExtension();
        DFABytecodeInterpreter interpreter(compiledExtension.bytecode(), compiledExtension.bytecodeLength(), contentExtension->m_pagesUsed);
        DFABytecodeInterpreter::Actions triggeredActions = interpreter.interpret(urlCString, flags);
        
        const SerializedActionByte* actions = compiledExtension.actions();
        const unsigned actionsLength = compiledExtension.actionsLength();
        
        bool sawIgnorePreviousRules = false;
        if (!triggeredActions.isEmpty()) {
            Vector<unsigned> actionLocations;
            actionLocations.reserveInitialCapacity(triggeredActions.size());
            for (auto actionLocation : triggeredActions)
                actionLocations.append(static_cast<unsigned>(actionLocation));
            std::sort(actionLocations.begin(), actionLocations.end());

            // Add actions in reverse order to properly deal with IgnorePreviousRules.
            for (unsigned i = actionLocations.size(); i; i--) {
                Action action = Action::deserialize(actions, actionsLength, actionLocations[i - 1]);
                if (action.type() == ActionType::IgnorePreviousRules) {
                    sawIgnorePreviousRules = true;
                    break;
                }
                finalActions.append(action);
            }
        }
        if (!sawIgnorePreviousRules) {
            DFABytecodeInterpreter::Actions universalActions = interpreter.actionsFromDFARoot();
            for (auto actionLocation : universalActions) {
                Action action = Action::deserialize(actions, actionsLength, static_cast<unsigned>(actionLocation));
                
                // CSS selectors were already compiled into a stylesheet using globalDisplayNoneSelectors.
                if (action.type() != ActionType::CSSDisplayNoneSelector)
                    finalActions.append(action);
            }
            finalActions.append(Action(ActionType::CSSDisplayNoneStyleSheet, contentExtension->identifier()));
        }
    }
    return finalActions;
}

StyleSheetContents* ContentExtensionsBackend::globalDisplayNoneStyleSheet(const String& identifier) const
{
    const auto& contentExtension = m_contentExtensions.get(identifier);
    return contentExtension ? contentExtension->globalDisplayNoneStyleSheet() : nullptr;
}

void ContentExtensionsBackend::processContentExtensionRulesForLoad(ResourceRequest& request, ResourceType resourceType, DocumentLoader& initiatingDocumentLoader)
{
    Document* currentDocument = nullptr;
    URL mainDocumentURL;

    if (initiatingDocumentLoader.frame()) {
        currentDocument = initiatingDocumentLoader.frame()->document();

        if (Document* mainDocument = initiatingDocumentLoader.frame()->mainFrame().document())
            mainDocumentURL = mainDocument->url();
    }

    ResourceLoadInfo resourceLoadInfo = { request.url(), mainDocumentURL, resourceType };
    Vector<ContentExtensions::Action> actions = actionsForResourceLoad(resourceLoadInfo);

    StringBuilder css;
    bool willBlockLoad = false;
    for (const auto& action : actions) {
        switch (action.type()) {
        case ContentExtensions::ActionType::BlockLoad:
            willBlockLoad = true;
            break;
        case ContentExtensions::ActionType::BlockCookies:
            request.setAllowCookies(false);
            break;
        case ContentExtensions::ActionType::CSSDisplayNoneSelector:
            css.append(action.stringArgument());
            css.append(displayNoneCSSRule());
            break;
        case ContentExtensions::ActionType::CSSDisplayNoneStyleSheet: {
            StyleSheetContents* styleSheetContents = globalDisplayNoneStyleSheet(action.stringArgument());
            if (styleSheetContents) {
                if (resourceType == ResourceType::Document)
                    initiatingDocumentLoader.addPendingContentExtensionSheet(action.stringArgument(), *styleSheetContents);
                else if (currentDocument)
                    currentDocument->styleSheetCollection().maybeAddContentExtensionSheet(action.stringArgument(), *styleSheetContents);
            }
            break;
        }
        case ContentExtensions::ActionType::IgnorePreviousRules:
        case ContentExtensions::ActionType::InvalidAction:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    if (css.length()) {
        Ref<StyleSheetContents> styleSheet = StyleSheetContents::create();
        styleSheet->setIsUserStyleSheet(true);

        if (styleSheet->parseString(css.toString())) {
            if (resourceType == ResourceType::Document)
                initiatingDocumentLoader.addPendingContentExtensionSheet(styleSheet);
            else if (currentDocument)
                currentDocument->styleSheetCollection().addUserSheet(WTF::move(styleSheet));
        }
    }

    if (willBlockLoad)
        request = ResourceRequest();
}

const String& ContentExtensionsBackend::displayNoneCSSRule()
{
    static NeverDestroyed<const String> rule(ASCIILiteral("{display:none !important;}\n"));
    return rule;
}

} // namespace ContentExtensions

} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
