/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

#include "Chrome.h"
#include "ChromeClient.h"
#include "CompiledContentExtension.h"
#include "ContentExtension.h"
#include "ContentExtensionsDebugging.h"
#include "DFABytecodeInterpreter.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "ExtensionStyleSheets.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "Page.h"
#include "ResourceLoadInfo.h"
#include <wtf/URL.h>
#include "UserContentController.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/CString.h>

namespace WebCore {

namespace ContentExtensions {
    
void ContentExtensionsBackend::addContentExtension(const String& identifier, Ref<CompiledContentExtension> compiledContentExtension, ContentExtension::ShouldCompileCSS shouldCompileCSS)
{
    ASSERT(!identifier.isEmpty());
    if (identifier.isEmpty())
        return;
    
    auto contentExtension = ContentExtension::create(identifier, WTFMove(compiledContentExtension), shouldCompileCSS);
    m_contentExtensions.set(identifier, WTFMove(contentExtension));
}

void ContentExtensionsBackend::removeContentExtension(const String& identifier)
{
    m_contentExtensions.remove(identifier);
}

void ContentExtensionsBackend::removeAllContentExtensions()
{
    m_contentExtensions.clear();
}

std::pair<Vector<Action>, Vector<String>> ContentExtensionsBackend::actionsForResourceLoad(const ResourceLoadInfo& resourceLoadInfo) const
{
#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    MonotonicTime addedTimeStart = MonotonicTime::now();
#endif
    if (m_contentExtensions.isEmpty()
        || !resourceLoadInfo.resourceURL.isValid()
        || resourceLoadInfo.resourceURL.protocolIsData())
        return { };

    const String& urlString = resourceLoadInfo.resourceURL.string();
    ASSERT_WITH_MESSAGE(urlString.isAllASCII(), "A decoded URL should only contain ASCII characters. The matching algorithm assumes the input is ASCII.");
    const auto urlCString = urlString.utf8();

    Vector<Action> finalActions;
    Vector<String> stylesheetIdentifiers;
    ResourceFlags flags = resourceLoadInfo.getResourceFlags();
    for (auto& contentExtension : m_contentExtensions.values()) {
        const CompiledContentExtension& compiledExtension = contentExtension->compiledExtension();
        
        DFABytecodeInterpreter withoutConditionsInterpreter(compiledExtension.filtersWithoutConditionsBytecode(), compiledExtension.filtersWithoutConditionsBytecodeLength());
        DFABytecodeInterpreter::Actions withoutConditionsActions = withoutConditionsInterpreter.interpret(urlCString, flags);
        
        URL topURL = resourceLoadInfo.mainDocumentURL;
        DFABytecodeInterpreter withConditionsInterpreter(compiledExtension.filtersWithConditionsBytecode(), compiledExtension.filtersWithConditionsBytecodeLength());
        DFABytecodeInterpreter::Actions withConditionsActions = withConditionsInterpreter.interpretWithConditions(urlCString, flags, contentExtension->topURLActions(topURL));
        
        const SerializedActionByte* actions = compiledExtension.actions();
        const unsigned actionsLength = compiledExtension.actionsLength();
        
        bool sawIgnorePreviousRules = false;
        const Vector<uint32_t>& universalWithConditions = contentExtension->universalActionsWithConditions(topURL);
        const Vector<uint32_t>& universalWithoutConditions = contentExtension->universalActionsWithoutConditions();
        if (!withoutConditionsActions.isEmpty() || !withConditionsActions.isEmpty() || !universalWithConditions.isEmpty() || !universalWithoutConditions.isEmpty()) {
            Vector<uint32_t> actionLocations;
            actionLocations.reserveInitialCapacity(withoutConditionsActions.size() + withConditionsActions.size() + universalWithoutConditions.size() + universalWithConditions.size());
            for (uint64_t actionLocation : withoutConditionsActions)
                actionLocations.uncheckedAppend(static_cast<uint32_t>(actionLocation));
            for (uint64_t actionLocation : withConditionsActions)
                actionLocations.uncheckedAppend(static_cast<uint32_t>(actionLocation));
            for (uint32_t actionLocation : universalWithoutConditions)
                actionLocations.uncheckedAppend(actionLocation);
            for (uint32_t actionLocation : universalWithConditions)
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
        if (!sawIgnorePreviousRules)
            stylesheetIdentifiers.append(contentExtension->identifier());
    }
#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    MonotonicTime addedTimeEnd = MonotonicTime::now();
    dataLogF("Time added: %f microseconds %s \n", (addedTimeEnd - addedTimeStart).microseconds(), resourceLoadInfo.resourceURL.string().utf8().data());
#endif
    return { WTFMove(finalActions), WTFMove(stylesheetIdentifiers) };
}

void ContentExtensionsBackend::forEach(const WTF::Function<void(const String&, ContentExtension&)>& apply)
{
    for (auto& pair : m_contentExtensions)
        apply(pair.key, pair.value);
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
    auto actions = actionsForResourceLoad(resourceLoadInfo);

    bool willBlockLoad = false;
    bool willBlockCookies = false;
    bool willMakeHTTPS = false;
    HashSet<std::pair<String, String>> notifications;
    for (const auto& action : actions.first) {
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
        case ContentExtensions::ActionType::Notify:
            notifications.add(std::make_pair(action.extensionIdentifier(), action.stringArgument()));
            break;
        case ContentExtensions::ActionType::MakeHTTPS: {
            if ((url.protocolIs("http") || url.protocolIs("ws"))
                && (!url.port() || WTF::isDefaultPortForProtocol(url.port().value(), url.protocol())))
                willMakeHTTPS = true;
            break;
        }
        case ContentExtensions::ActionType::IgnorePreviousRules:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    for (const auto& identifier : actions.second) {
        if (auto* styleSheetContents = globalDisplayNoneStyleSheet(identifier)) {
            if (resourceType == ResourceType::Document)
                initiatingDocumentLoader.addPendingContentExtensionSheet(identifier, *styleSheetContents);
            else if (currentDocument)
                currentDocument->extensionStyleSheets().maybeAddContentExtensionSheet(identifier, *styleSheetContents);
        }
    }

    if (currentDocument) {
        if (willMakeHTTPS) {
            ASSERT(url.protocolIs("http") || url.protocolIs("ws"));
            String newProtocol = url.protocolIs("http") ? "https"_s : "wss"_s;
            currentDocument->addConsoleMessage(MessageSource::ContentBlocker, MessageLevel::Info, makeString("Content blocker promoted URL from ", url.string(), " to ", newProtocol));
        }
        if (willBlockLoad)
            currentDocument->addConsoleMessage(MessageSource::ContentBlocker, MessageLevel::Info, makeString("Content blocker prevented frame displaying ", mainDocumentURL.string(), " from loading a resource from ", url.string()));
    }
    return { willBlockLoad, willBlockCookies, willMakeHTTPS, WTFMove(notifications) };
}

BlockedStatus ContentExtensionsBackend::processContentExtensionRulesForPingLoad(const URL& url, const URL& mainDocumentURL)
{
    if (m_contentExtensions.isEmpty())
        return { };

    ResourceLoadInfo resourceLoadInfo = { url, mainDocumentURL, ResourceType::Raw };
    auto actions = actionsForResourceLoad(resourceLoadInfo);

    bool willBlockLoad = false;
    bool willBlockCookies = false;
    bool willMakeHTTPS = false;
    for (const auto& action : actions.first) {
        switch (action.type()) {
        case ContentExtensions::ActionType::BlockLoad:
            willBlockLoad = true;
            break;
        case ContentExtensions::ActionType::BlockCookies:
            willBlockCookies = true;
            break;
        case ContentExtensions::ActionType::MakeHTTPS:
            if ((url.protocolIs("http") || url.protocolIs("ws")) && (!url.port() || WTF::isDefaultPortForProtocol(url.port().value(), url.protocol())))
                willMakeHTTPS = true;
            break;
        case ContentExtensions::ActionType::CSSDisplayNoneSelector:
        case ContentExtensions::ActionType::Notify:
            break;
        case ContentExtensions::ActionType::IgnorePreviousRules:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    return { willBlockLoad, willBlockCookies, willMakeHTTPS, { } };
}

const String& ContentExtensionsBackend::displayNoneCSSRule()
{
    static NeverDestroyed<const String> rule(MAKE_STATIC_STRING_IMPL("display:none !important;"));
    return rule;
}

void applyBlockedStatusToRequest(const BlockedStatus& status, Page* page, ResourceRequest& request)
{
    if (page && !status.notifications.isEmpty())
        page->chrome().client().contentRuleListNotification(request.url(), status.notifications);

    if (status.blockedCookies)
        request.setAllowCookies(false);

    if (status.madeHTTPS) {
        const URL& originalURL = request.url();
        ASSERT(originalURL.protocolIs("http"));
        ASSERT(!originalURL.port() || WTF::isDefaultPortForProtocol(originalURL.port().value(), originalURL.protocol()));

        URL newURL = originalURL;
        newURL.setProtocol("https");
        if (originalURL.port())
            newURL.setPort(WTF::defaultPortForProtocol("https").value());
        request.setURL(newURL);
    }
}
    
} // namespace ContentExtensions

} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
