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
#include "ContentExtensionsDebugging.h"
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
#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    double addedTimeStart = monotonicallyIncreasingTime();
#endif
    if (resourceLoadInfo.resourceURL.protocolIsData())
        return Vector<Action>();

    const String& urlString = resourceLoadInfo.resourceURL.string();
    ASSERT_WITH_MESSAGE(urlString.containsOnlyASCII(), "A decoded URL should only contain ASCII characters. The matching algorithm assumes the input is ASCII.");
    const CString& urlCString = urlString.utf8();

    Vector<Action> finalActions;
    ResourceFlags flags = resourceLoadInfo.getResourceFlags();
    for (auto& contentExtension : m_contentExtensions.values()) {
        RELEASE_ASSERT(contentExtension);
        const CompiledContentExtension& compiledExtension = contentExtension->compiledExtension();
        
        // FIXME: These should use a different Vector<bool> to keep track of which memory pages are used when doing memory reporting. Or just remove the memory reporting completely.
        DFABytecodeInterpreter withoutDomainsInterpreter(compiledExtension.filtersWithoutDomainsBytecode(), compiledExtension.filtersWithoutDomainsBytecodeLength(), contentExtension->m_pagesUsed);
        DFABytecodeInterpreter::Actions triggeredActions = withoutDomainsInterpreter.interpret(urlCString, flags);
        
        // Check to see if there are any actions triggered with if- or unless-domain and check the domain if there are.
        DFABytecodeInterpreter withDomainsInterpreter(compiledExtension.filtersWithDomainsBytecode(), compiledExtension.filtersWithDomainsBytecodeLength(), contentExtension->m_pagesUsed);
        
        DFABytecodeInterpreter::Actions withDomainsPossibleActions = withDomainsInterpreter.interpret(urlCString, flags);
        if (!withDomainsPossibleActions.isEmpty()) {
            DFABytecodeInterpreter domainsInterpreter(compiledExtension.domainFiltersBytecode(), compiledExtension.domainFiltersBytecodeLength(), contentExtension->m_pagesUsed);
            DFABytecodeInterpreter::Actions domainsActions = domainsInterpreter.interpret(resourceLoadInfo.mainDocumentURL.host().utf8(), flags);
            
            DFABytecodeInterpreter::Actions ifDomainActions;
            DFABytecodeInterpreter::Actions unlessDomainActions;
            for (uint64_t action : domainsActions) {
                if (action & IfDomainFlag)
                    ifDomainActions.add(action);
                else
                    unlessDomainActions.add(action);
            }
            
            for (uint64_t action : withDomainsPossibleActions) {
                if (ifDomainActions.contains(action)) {
                    // If an if-domain trigger matches, add the action.
                    ASSERT(action & IfDomainFlag);
                    triggeredActions.add(action & ~IfDomainFlag);
                } else if (!(action & IfDomainFlag) && !unlessDomainActions.contains(action)) {
                    // If this action did not need an if-domain, it must have been an unless-domain rule.
                    // Add the action unless it matched an unless-domain trigger.
                    triggeredActions.add(action);
                }
            }
        }
        
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
                action.setExtensionIdentifier(contentExtension->identifier());
                if (action.type() == ActionType::IgnorePreviousRules) {
                    sawIgnorePreviousRules = true;
                    break;
                }
                finalActions.append(action);
            }
        }
        if (!sawIgnorePreviousRules) {
            DFABytecodeInterpreter::Actions universalActions = withoutDomainsInterpreter.actionsFromDFARoot();
            for (uint64_t actionLocation : universalActions) {
                // FIXME: We shouldn't deserialize an action all the way if it is a css-display-none selector.
                Action action = Action::deserialize(actions, actionsLength, static_cast<unsigned>(actionLocation));
                action.setExtensionIdentifier(contentExtension->identifier());

                // CSS selectors were already compiled into a stylesheet using globalDisplayNoneSelectors.
                if (action.type() != ActionType::CSSDisplayNoneSelector)
                    finalActions.append(action);
            }
            finalActions.append(Action(ActionType::CSSDisplayNoneStyleSheet, contentExtension->identifier()));
            finalActions.last().setExtensionIdentifier(contentExtension->identifier());
        }
    }
#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    double addedTimeEnd = monotonicallyIncreasingTime();
    WTFLogAlways("Time added: %f microseconds %s", (addedTimeEnd - addedTimeStart) * 1.0e6, resourceLoadInfo.resourceURL.string().utf8().data());
#endif
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
            if (resourceType == ResourceType::Document)
                initiatingDocumentLoader.addPendingContentExtensionDisplayNoneSelector(action.extensionIdentifier(), action.stringArgument(), action.actionID());
            else if (currentDocument)
                currentDocument->styleSheetCollection().addDisplayNoneSelector(action.extensionIdentifier(), action.stringArgument(), action.actionID());
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

    if (willBlockLoad)
        request = ResourceRequest();
}

const String& ContentExtensionsBackend::displayNoneCSSRule()
{
    static NeverDestroyed<const String> rule(ASCIILiteral("display:none !important;"));
    return rule;
}

} // namespace ContentExtensions

} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
