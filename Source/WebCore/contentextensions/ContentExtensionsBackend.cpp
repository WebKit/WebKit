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
#include "ContentRuleListResults.h"
#include "CustomHeaderFields.h"
#include "DFABytecodeInterpreter.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "ExtensionStyleSheets.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "Page.h"
#include "ResourceLoadInfo.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"
#include "Settings.h"
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

auto ContentExtensionsBackend::actionsForResourceLoad(const ResourceLoadInfo& resourceLoadInfo) const -> Vector<ActionsFromContentRuleList>
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

    Vector<ActionsFromContentRuleList> actionsVector;
    actionsVector.reserveInitialCapacity(m_contentExtensions.size());
    const ResourceFlags flags = resourceLoadInfo.getResourceFlags();
    for (auto& contentExtension : m_contentExtensions.values()) {
        ActionsFromContentRuleList actionsStruct;
        actionsStruct.contentRuleListIdentifier = contentExtension->identifier();

        const CompiledContentExtension& compiledExtension = contentExtension->compiledExtension();
        
        DFABytecodeInterpreter withoutConditionsInterpreter(compiledExtension.filtersWithoutConditionsBytecode(), compiledExtension.filtersWithoutConditionsBytecodeLength());
        DFABytecodeInterpreter::Actions withoutConditionsActions = withoutConditionsInterpreter.interpret(urlCString, flags);
        
        URL topURL = resourceLoadInfo.mainDocumentURL;
        DFABytecodeInterpreter withConditionsInterpreter(compiledExtension.filtersWithConditionsBytecode(), compiledExtension.filtersWithConditionsBytecodeLength());
        DFABytecodeInterpreter::Actions withConditionsActions = withConditionsInterpreter.interpretWithConditions(urlCString, flags, contentExtension->topURLActions(topURL));
        
        const SerializedActionByte* actions = compiledExtension.actions();
        const unsigned actionsLength = compiledExtension.actionsLength();
        
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
                if (action.type() == ActionType::IgnorePreviousRules) {
                    actionsStruct.sawIgnorePreviousRules = true;
                    break;
                }
                actionsStruct.actions.append(WTFMove(action));
            }
        }
        actionsVector.uncheckedAppend(WTFMove(actionsStruct));
    }
#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    MonotonicTime addedTimeEnd = MonotonicTime::now();
    dataLogF("Time added: %f microseconds %s \n", (addedTimeEnd - addedTimeStart).microseconds(), resourceLoadInfo.resourceURL.string().utf8().data());
#endif
    return actionsVector;
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

ContentRuleListResults ContentExtensionsBackend::processContentRuleListsForLoad(const URL& url, OptionSet<ResourceType> resourceType, DocumentLoader& initiatingDocumentLoader)
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

    ContentRuleListResults results;
    results.results.reserveInitialCapacity(actions.size());
    for (const auto& actionsFromContentRuleList : actions) {
        const String& contentRuleListIdentifier = actionsFromContentRuleList.contentRuleListIdentifier;
        ContentRuleListResults::Result result;
        for (const auto& action : actionsFromContentRuleList.actions) {
            switch (action.type()) {
            case ContentExtensions::ActionType::BlockLoad:
                results.summary.blockedLoad = true;
                result.blockedLoad = true;
                break;
            case ContentExtensions::ActionType::BlockCookies:
                results.summary.blockedCookies = true;
                result.blockedCookies = true;
                break;
            case ContentExtensions::ActionType::CSSDisplayNoneSelector:
                if (resourceType == ResourceType::Document)
                    initiatingDocumentLoader.addPendingContentExtensionDisplayNoneSelector(contentRuleListIdentifier, action.stringArgument(), action.actionID());
                else if (currentDocument)
                    currentDocument->extensionStyleSheets().addDisplayNoneSelector(contentRuleListIdentifier, action.stringArgument(), action.actionID());
                break;
            case ContentExtensions::ActionType::Notify:
                results.summary.hasNotifications = true;
                result.notifications.append(action.stringArgument());
                break;
            case ContentExtensions::ActionType::MakeHTTPS: {
                if ((url.protocolIs("http") || url.protocolIs("ws"))
                    && (!url.port() || WTF::isDefaultPortForProtocol(url.port().value(), url.protocol()))) {
                    results.summary.madeHTTPS = true;
                    result.madeHTTPS = true;
                }
                break;
            }
            case ContentExtensions::ActionType::IgnorePreviousRules:
                RELEASE_ASSERT_NOT_REACHED();
            }
        }

        if (!actionsFromContentRuleList.sawIgnorePreviousRules) {
            if (auto* styleSheetContents = globalDisplayNoneStyleSheet(contentRuleListIdentifier)) {
                if (resourceType == ResourceType::Document)
                    initiatingDocumentLoader.addPendingContentExtensionSheet(contentRuleListIdentifier, *styleSheetContents);
                else if (currentDocument)
                    currentDocument->extensionStyleSheets().maybeAddContentExtensionSheet(contentRuleListIdentifier, *styleSheetContents);
            }
        }

        results.results.uncheckedAppend({ contentRuleListIdentifier, WTFMove(result) });
    }

    if (currentDocument) {
        if (results.summary.madeHTTPS) {
            ASSERT(url.protocolIs("http") || url.protocolIs("ws"));
            String newProtocol = url.protocolIs("http") ? "https"_s : "wss"_s;
            currentDocument->addConsoleMessage(MessageSource::ContentBlocker, MessageLevel::Info, makeString("Content blocker promoted URL from ", url.string(), " to ", newProtocol));
        }
        if (results.summary.blockedLoad) {
            currentDocument->addConsoleMessage(MessageSource::ContentBlocker, MessageLevel::Info, makeString("Content blocker prevented frame displaying ", mainDocumentURL.string(), " from loading a resource from ", url.string()));
        
            // Quirk for content-blocker interference with Google's anti-flicker optimization (rdar://problem/45968770).
            // https://developers.google.com/optimize/
            if (currentDocument->settings().googleAntiFlickerOptimizationQuirkEnabled()
                && ((equalLettersIgnoringASCIICase(url.host(), "www.google-analytics.com") && equalLettersIgnoringASCIICase(url.path(), "/analytics.js"))
                    || (equalLettersIgnoringASCIICase(url.host(), "www.googletagmanager.com") && equalLettersIgnoringASCIICase(url.path(), "/gtm.js")))) {
                if (auto* frame = currentDocument->frame())
                    frame->script().evaluateIgnoringException(ScriptSourceCode { "try { window.dataLayer.hide.end(); console.log('Called window.dataLayer.hide.end() in frame ' + document.URL + ' because the content blocker blocked the load of the https://www.google-analytics.com/analytics.js script'); } catch (e) { }"_s });
            }
        }
    }

    return results;
}

ContentRuleListResults ContentExtensionsBackend::processContentRuleListsForPingLoad(const URL& url, const URL& mainDocumentURL)
{
    if (m_contentExtensions.isEmpty())
        return { };

    ResourceLoadInfo resourceLoadInfo = { url, mainDocumentURL, ResourceType::Raw };
    auto actions = actionsForResourceLoad(resourceLoadInfo);

    ContentRuleListResults results;
    for (const auto& actionsFromContentRuleList : actions) {
        for (const auto& action : actionsFromContentRuleList.actions) {
            switch (action.type()) {
            case ContentExtensions::ActionType::BlockLoad:
                results.summary.blockedLoad = true;
                break;
            case ContentExtensions::ActionType::BlockCookies:
                results.summary.blockedCookies = true;
                break;
            case ContentExtensions::ActionType::MakeHTTPS:
                if ((url.protocolIs("http") || url.protocolIs("ws")) && (!url.port() || WTF::isDefaultPortForProtocol(url.port().value(), url.protocol())))
                    results.summary.madeHTTPS = true;
                break;
            case ContentExtensions::ActionType::CSSDisplayNoneSelector:
            case ContentExtensions::ActionType::Notify:
                // We currently have not implemented notifications from the NetworkProcess to the UIProcess.
                break;
            case ContentExtensions::ActionType::IgnorePreviousRules:
                RELEASE_ASSERT_NOT_REACHED();
            }
        }
    }

    return results;
}

const String& ContentExtensionsBackend::displayNoneCSSRule()
{
    static NeverDestroyed<const String> rule(MAKE_STATIC_STRING_IMPL("display:none !important;"));
    return rule;
}

void applyResultsToRequest(ContentRuleListResults&& results, Page* page, ResourceRequest& request)
{
    if (results.summary.blockedCookies)
        request.setAllowCookies(false);

    if (results.summary.madeHTTPS) {
        const URL& originalURL = request.url();
        ASSERT(originalURL.protocolIs("http"));
        ASSERT(!originalURL.port() || WTF::isDefaultPortForProtocol(originalURL.port().value(), originalURL.protocol()));

        URL newURL = originalURL;
        newURL.setProtocol("https");
        if (originalURL.port())
            newURL.setPort(WTF::defaultPortForProtocol("https").value());
        request.setURL(newURL);
    }

    if (page && results.shouldNotifyApplication()) {
        results.results.removeAllMatching([](const auto& pair) {
            return !pair.second.shouldNotifyApplication();
        });
        page->chrome().client().contentRuleListNotification(request.url(), results);
    }
}
    
} // namespace ContentExtensions

} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
