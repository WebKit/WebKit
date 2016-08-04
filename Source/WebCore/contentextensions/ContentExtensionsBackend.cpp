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
#include "ExtensionStyleSheets.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
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
    m_contentExtensions.set(identifier, WTFMove(extension));
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
        
        DFABytecodeInterpreter withoutDomainsInterpreter(compiledExtension.filtersWithoutDomainsBytecode(), compiledExtension.filtersWithoutDomainsBytecodeLength());
        DFABytecodeInterpreter::Actions withoutDomainsActions = withoutDomainsInterpreter.interpret(urlCString, flags);
        
        String domain = resourceLoadInfo.mainDocumentURL.host();
        DFABytecodeInterpreter withDomainsInterpreter(compiledExtension.filtersWithDomainsBytecode(), compiledExtension.filtersWithDomainsBytecodeLength());
        DFABytecodeInterpreter::Actions withDomainsActions = withDomainsInterpreter.interpretWithDomains(urlCString, flags, contentExtension->cachedDomainActions(domain));
        
        const SerializedActionByte* actions = compiledExtension.actions();
        const unsigned actionsLength = compiledExtension.actionsLength();
        
        bool sawIgnorePreviousRules = false;
        const Vector<uint32_t>& universalWithDomains = contentExtension->universalActionsWithDomains(domain);
        const Vector<uint32_t>& universalWithoutDomains = contentExtension->universalActionsWithoutDomains();
        if (!withoutDomainsActions.isEmpty() || !withDomainsActions.isEmpty() || !universalWithDomains.isEmpty() || !universalWithoutDomains.isEmpty()) {
            Vector<uint32_t> actionLocations;
            actionLocations.reserveInitialCapacity(withoutDomainsActions.size() + withDomainsActions.size() + universalWithoutDomains.size() + universalWithDomains.size());
            for (uint64_t actionLocation : withoutDomainsActions)
                actionLocations.uncheckedAppend(static_cast<uint32_t>(actionLocation));
            for (uint64_t actionLocation : withDomainsActions)
                actionLocations.uncheckedAppend(static_cast<uint32_t>(actionLocation));
            for (uint32_t actionLocation : universalWithoutDomains)
                actionLocations.uncheckedAppend(actionLocation);
            for (uint32_t actionLocation : universalWithDomains)
                actionLocations.uncheckedAppend(actionLocation);
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
            finalActions.append(Action(ActionType::CSSDisplayNoneStyleSheet, contentExtension->identifier()));
            finalActions.last().setExtensionIdentifier(contentExtension->identifier());
        }
    }
#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    double addedTimeEnd = monotonicallyIncreasingTime();
    dataLogF("Time added: %f microseconds %s \n", (addedTimeEnd - addedTimeStart) * 1.0e6, resourceLoadInfo.resourceURL.string().utf8().data());
#endif
    return finalActions;
}

StyleSheetContents* ContentExtensionsBackend::globalDisplayNoneStyleSheet(const String& identifier) const
{
    const auto& contentExtension = m_contentExtensions.get(identifier);
    return contentExtension ? contentExtension->globalDisplayNoneStyleSheet() : nullptr;
}

BlockedStatus ContentExtensionsBackend::processContentExtensionRulesForLoad(const URL& url, ResourceType resourceType, DocumentLoader& initiatingDocumentLoader)
{
    if (m_contentExtensions.isEmpty())
        return { };

    Document* currentDocument = nullptr;
    URL mainDocumentURL;

    if (Frame* frame = initiatingDocumentLoader.frame()) {
        currentDocument = frame->document();

        if (initiatingDocumentLoader.isLoadingMainResource()
            && frame->isMainFrame()
            && resourceType == ResourceType::Document)
            mainDocumentURL = url;
        else if (Document* mainDocument = frame->mainFrame().document())
            mainDocumentURL = mainDocument->url();
    }

    ResourceLoadInfo resourceLoadInfo = { url, mainDocumentURL, resourceType };
    Vector<ContentExtensions::Action> actions = actionsForResourceLoad(resourceLoadInfo);

    bool willBlockLoad = false;
    bool willBlockCookies = false;
    bool willMakeHTTPS = false;
    for (const auto& action : actions) {
        switch (action.type()) {
        case ContentExtensions::ActionType::BlockLoad:
            willBlockLoad = true;
            break;
        case ContentExtensions::ActionType::BlockCookies:
            willBlockCookies = true;
            break;
        case ContentExtensions::ActionType::CSSDisplayNoneSelector:
            if (resourceType == ResourceType::Document)
                initiatingDocumentLoader.addPendingContentExtensionDisplayNoneSelector(action.extensionIdentifier(), action.stringArgument(), action.actionID());
            else if (currentDocument)
                currentDocument->extensionStyleSheets().addDisplayNoneSelector(action.extensionIdentifier(), action.stringArgument(), action.actionID());
            break;
        case ContentExtensions::ActionType::CSSDisplayNoneStyleSheet: {
            StyleSheetContents* styleSheetContents = globalDisplayNoneStyleSheet(action.stringArgument());
            if (styleSheetContents) {
                if (resourceType == ResourceType::Document)
                    initiatingDocumentLoader.addPendingContentExtensionSheet(action.stringArgument(), *styleSheetContents);
                else if (currentDocument)
                    currentDocument->extensionStyleSheets().maybeAddContentExtensionSheet(action.stringArgument(), *styleSheetContents);
            }
            break;
        }
        case ContentExtensions::ActionType::MakeHTTPS: {
            if ((url.protocolIs("http") || url.protocolIs("ws"))
                && (!url.hasPort() || isDefaultPortForProtocol(url.port(), url.protocol())))
                willMakeHTTPS = true;
            break;
        }
        case ContentExtensions::ActionType::IgnorePreviousRules:
        case ContentExtensions::ActionType::InvalidAction:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    if (currentDocument) {
        if (willMakeHTTPS) {
            ASSERT(url.protocolIs("http") || url.protocolIs("ws"));
            String newProtocol = url.protocolIs("http") ? ASCIILiteral("https") : ASCIILiteral("wss");
            currentDocument->addConsoleMessage(MessageSource::ContentBlocker, MessageLevel::Info, makeString("Content blocker promoted URL from ", url.string(), " to ", newProtocol));
        }
        if (willBlockLoad)
            currentDocument->addConsoleMessage(MessageSource::ContentBlocker, MessageLevel::Info, makeString("Content blocker prevented frame displaying ", mainDocumentURL.string(), " from loading a resource from ", url.string()));
    }
    return { willBlockLoad, willBlockCookies, willMakeHTTPS };
}

const String& ContentExtensionsBackend::displayNoneCSSRule()
{
    static NeverDestroyed<const String> rule(ASCIILiteral("display:none !important;"));
    return rule;
}

void applyBlockedStatusToRequest(const BlockedStatus& status, ResourceRequest& request)
{
    if (status.blockedCookies)
        request.setAllowCookies(false);

    if (status.madeHTTPS) {
        const URL& originalURL = request.url();
        ASSERT(originalURL.protocolIs("http"));
        ASSERT(!originalURL.hasPort() || isDefaultPortForProtocol(originalURL.port(), originalURL.protocol()));

        URL newURL = originalURL;
        newURL.setProtocol("https");
        if (originalURL.hasPort())
            newURL.setPort(defaultPortForProtocol("https"));
        request.setURL(newURL);
    }
}
    
} // namespace ContentExtensions

} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
