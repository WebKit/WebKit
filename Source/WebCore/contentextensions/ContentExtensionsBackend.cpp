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

namespace WebCore::ContentExtensions {

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/ContentRuleListAdditions.mm>
#else
static void makeSecureIfNecessary(ContentRuleListResults& results, const URL& url, const URL& redirectFrom = { })
{
    if (redirectFrom.host() == url.host() && redirectFrom.protocolIs("https"_s))
        return;

    if (!url.protocolIs("http"_s))
        return;
    if (url.host() == "www.opengl.org"_s
        || url.host() == "webkit.org"_s
        || url.host() == "download"_s)
        results.summary.madeHTTPS = true;
}

std::optional<String> customLoadBlockingMessageForConsole(const ContentRuleListResults&, const URL&, const URL&)
{
    return std::nullopt;
}
#endif

bool ContentExtensionsBackend::shouldBeMadeSecure(const URL& url)
{
    ContentRuleListResults results;
    makeSecureIfNecessary(results, url);
    return results.summary.madeHTTPS;
}

void ContentExtensionsBackend::addContentExtension(const String& identifier, Ref<CompiledContentExtension> compiledContentExtension, URL&& extensionBaseURL, ContentExtension::ShouldCompileCSS shouldCompileCSS)
{
    ASSERT(!identifier.isEmpty());
    if (identifier.isEmpty())
        return;
    
    auto contentExtension = ContentExtension::create(identifier, WTFMove(compiledContentExtension), WTFMove(extensionBaseURL), shouldCompileCSS);
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

auto ContentExtensionsBackend::actionsFromContentRuleList(const ContentExtension& contentExtension, const String& urlString, const ResourceLoadInfo& resourceLoadInfo, ResourceFlags flags) const -> ActionsFromContentRuleList
{
    ActionsFromContentRuleList actionsStruct;
    actionsStruct.contentRuleListIdentifier = contentExtension.identifier();

    const auto& compiledExtension = contentExtension.compiledExtension();

    DFABytecodeInterpreter interpreter(compiledExtension.urlFiltersBytecode());
    auto actionLocations = interpreter.interpret(urlString, flags);
    auto& topURLActions = contentExtension.topURLActions(resourceLoadInfo.mainDocumentURL);
    auto& frameURLActions = contentExtension.frameURLActions(resourceLoadInfo.frameURL);

    actionLocations.removeIf([&](uint64_t actionAndFlags) {
        ResourceFlags flags = actionAndFlags >> 32;
        auto actionCondition = static_cast<ActionCondition>(flags & ActionConditionMask);
        switch (actionCondition) {
        case ActionCondition::None:
            return false;
        case ActionCondition::IfTopURL:
            return !topURLActions.contains(actionAndFlags);
        case ActionCondition::UnlessTopURL:
            return topURLActions.contains(actionAndFlags);
        case ActionCondition::IfFrameURL:
            return !frameURLActions.contains(actionAndFlags);
        }
        ASSERT_NOT_REACHED();
        return false;
    });

    auto serializedActions = compiledExtension.serializedActions();

    const auto& universalActions = contentExtension.universalActions();
    if (auto totalActionCount = actionLocations.size() + universalActions.size()) {
        Vector<uint32_t> vector;
        vector.reserveInitialCapacity(totalActionCount);
        for (uint64_t actionLocation : actionLocations)
            vector.uncheckedAppend(static_cast<uint32_t>(actionLocation));
        for (uint64_t actionLocation : universalActions)
            vector.uncheckedAppend(static_cast<uint32_t>(actionLocation));
        std::sort(vector.begin(), vector.end());

        // Add actions in reverse order to properly deal with IgnorePreviousRules.
        for (auto i = vector.size(); i; i--) {
            auto action = DeserializedAction::deserialize(serializedActions, vector[i - 1]);
            if (std::holds_alternative<IgnorePreviousRulesAction>(action.data())) {
                actionsStruct.sawIgnorePreviousRules = true;
                break;
            }
            actionsStruct.actions.append(WTFMove(action));
        }
    }
    return actionsStruct;
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

    Vector<ActionsFromContentRuleList> actionsVector;
    actionsVector.reserveInitialCapacity(m_contentExtensions.size());
    ASSERT(!(resourceLoadInfo.getResourceFlags() & ActionConditionMask));
    const ResourceFlags flags = resourceLoadInfo.getResourceFlags() | ActionConditionMask;
    for (auto& contentExtension : m_contentExtensions.values())
        actionsVector.uncheckedAppend(actionsFromContentRuleList(contentExtension.get(), urlString, resourceLoadInfo, flags));
#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
    MonotonicTime addedTimeEnd = MonotonicTime::now();
    dataLogF("Time added: %f microseconds %s \n", (addedTimeEnd - addedTimeStart).microseconds(), resourceLoadInfo.resourceURL.string().utf8().data());
#endif
    return actionsVector;
}

void ContentExtensionsBackend::forEach(const Function<void(const String&, ContentExtension&)>& apply)
{
    for (auto& pair : m_contentExtensions)
        apply(pair.key, pair.value);
}

StyleSheetContents* ContentExtensionsBackend::globalDisplayNoneStyleSheet(const String& identifier) const
{
    const auto& contentExtension = m_contentExtensions.get(identifier);
    return contentExtension ? contentExtension->globalDisplayNoneStyleSheet() : nullptr;
}

ContentRuleListResults ContentExtensionsBackend::processContentRuleListsForLoad(Page& page, const URL& url, OptionSet<ResourceType> resourceType, DocumentLoader& initiatingDocumentLoader, const URL& redirectFrom)
{
    Document* currentDocument = nullptr;
    URL mainDocumentURL;
    URL frameURL;
    bool mainFrameContext = false;

    if (auto* frame = initiatingDocumentLoader.frame()) {
        mainFrameContext = frame->isMainFrame();
        currentDocument = frame->document();

        if (initiatingDocumentLoader.isLoadingMainResource()
            && frame->isMainFrame()
            && resourceType == ResourceType::Document)
            mainDocumentURL = url;
        else if (auto* mainDocument = frame->mainFrame().document())
            mainDocumentURL = mainDocument->url();
    }
    if (currentDocument)
        frameURL = currentDocument->url();
    else
        frameURL = url;

    ResourceLoadInfo resourceLoadInfo { url, mainDocumentURL, frameURL, resourceType, mainFrameContext };
    auto actions = actionsForResourceLoad(resourceLoadInfo);

    ContentRuleListResults results;
    if (page.httpsUpgradeEnabled())
        makeSecureIfNecessary(results, url, redirectFrom);
    results.results.reserveInitialCapacity(actions.size());
    for (const auto& actionsFromContentRuleList : actions) {
        const String& contentRuleListIdentifier = actionsFromContentRuleList.contentRuleListIdentifier;
        ContentRuleListResults::Result result;
        for (const auto& action : actionsFromContentRuleList.actions) {
            std::visit(WTF::makeVisitor([&](const BlockLoadAction&) {
                results.summary.blockedLoad = true;
                result.blockedLoad = true;
            }, [&](const BlockCookiesAction&) {
                results.summary.blockedCookies = true;
                result.blockedCookies = true;
            }, [&](const CSSDisplayNoneSelectorAction& actionData) {
                if (resourceType == ResourceType::Document)
                    initiatingDocumentLoader.addPendingContentExtensionDisplayNoneSelector(contentRuleListIdentifier, actionData.string, action.actionID());
                else if (currentDocument)
                    currentDocument->extensionStyleSheets().addDisplayNoneSelector(contentRuleListIdentifier, actionData.string, action.actionID());
            }, [&](const NotifyAction& actionData) {
                results.summary.hasNotifications = true;
                result.notifications.append(actionData.string);
            }, [&](const MakeHTTPSAction&) {
                if ((url.protocolIs("http"_s) || url.protocolIs("ws"_s))
                    && (!url.port() || WTF::isDefaultPortForProtocol(url.port().value(), url.protocol()))) {
                    results.summary.madeHTTPS = true;
                    result.madeHTTPS = true;
                }
            }, [&](const IgnorePreviousRulesAction&) {
                RELEASE_ASSERT_NOT_REACHED();
            }, [&] (const ModifyHeadersAction& action) {
                if (initiatingDocumentLoader.allowsActiveContentRuleListActionsForURL(contentRuleListIdentifier, url)) {
                    result.modifiedHeaders = true;
                    results.summary.modifyHeadersActions.append(action);
                }
            }, [&] (const RedirectAction& redirectAction) {
                if (initiatingDocumentLoader.allowsActiveContentRuleListActionsForURL(contentRuleListIdentifier, url)) {
                    result.redirected = true;
                    results.summary.redirectActions.append({ redirectAction, m_contentExtensions.get(contentRuleListIdentifier)->extensionBaseURL() });
                }
            }), action.data());
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
            ASSERT(url.protocolIs("http"_s) || url.protocolIs("ws"_s));
            String newProtocol = url.protocolIs("http"_s) ? "https"_s : "wss"_s;
            currentDocument->addConsoleMessage(MessageSource::ContentBlocker, MessageLevel::Info, makeString("Promoted URL from ", url.string(), " to ", newProtocol));
        }
        if (results.summary.blockedLoad) {
            String consoleMessage;
            if (auto message = customLoadBlockingMessageForConsole(results, url, mainDocumentURL))
                consoleMessage = WTFMove(*message);
            else
                consoleMessage = makeString("Content blocker prevented frame displaying ", mainDocumentURL.string(), " from loading a resource from ", url.string());
            currentDocument->addConsoleMessage(MessageSource::ContentBlocker, MessageLevel::Info, WTFMove(consoleMessage));
        
            // Quirk for content-blocker interference with Google's anti-flicker optimization (rdar://problem/45968770).
            // https://developers.google.com/optimize/
            if (currentDocument->settings().googleAntiFlickerOptimizationQuirkEnabled()
                && ((equalLettersIgnoringASCIICase(url.host(), "www.google-analytics.com"_s) && equalLettersIgnoringASCIICase(url.path(), "/analytics.js"_s))
                    || (equalLettersIgnoringASCIICase(url.host(), "www.googletagmanager.com"_s) && equalLettersIgnoringASCIICase(url.path(), "/gtm.js"_s)))) {
                if (auto* frame = currentDocument->frame())
                    frame->script().evaluateIgnoringException(ScriptSourceCode { "try { window.dataLayer.hide.end(); console.log('Called window.dataLayer.hide.end() in frame ' + document.URL + ' because the content blocker blocked the load of the https://www.google-analytics.com/analytics.js script'); } catch (e) { }"_s });
            }
        }
    }

    return results;
}

ContentRuleListResults ContentExtensionsBackend::processContentRuleListsForPingLoad(const URL& url, const URL& mainDocumentURL, const URL& frameURL)
{
    ResourceLoadInfo resourceLoadInfo { url, mainDocumentURL, frameURL, ResourceType::Ping };
    auto actions = actionsForResourceLoad(resourceLoadInfo);

    ContentRuleListResults results;
    makeSecureIfNecessary(results, url);
    for (const auto& actionsFromContentRuleList : actions) {
        for (const auto& action : actionsFromContentRuleList.actions) {
            std::visit(WTF::makeVisitor([&](const BlockLoadAction&) {
                results.summary.blockedLoad = true;
            }, [&](const BlockCookiesAction&) {
                results.summary.blockedCookies = true;
            }, [&](const CSSDisplayNoneSelectorAction&) {
            }, [&](const NotifyAction&) {
                // We currently have not implemented notifications from the NetworkProcess to the UIProcess.
            }, [&](const MakeHTTPSAction&) {
                if ((url.protocolIs("http"_s) || url.protocolIs("ws"_s)) && (!url.port() || WTF::isDefaultPortForProtocol(url.port().value(), url.protocol())))
                    results.summary.madeHTTPS = true;
            }, [&](const IgnorePreviousRulesAction&) {
                RELEASE_ASSERT_NOT_REACHED();
            }, [&] (const ModifyHeadersAction&) {
                // We currently have not implemented active actions from the network process (CORS preflight).
            }, [&] (const RedirectAction&) {
                // We currently have not implemented active actions from the network process (CORS preflight).
            }), action.data());
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
        ASSERT(originalURL.protocolIs("http"_s));
        ASSERT(!originalURL.port() || WTF::isDefaultPortForProtocol(originalURL.port().value(), originalURL.protocol()));

        URL newURL = originalURL;
        newURL.setProtocol("https"_s);
        if (originalURL.port())
            newURL.setPort(WTF::defaultPortForProtocol("https"_s).value());
        request.setURL(newURL);
    }

    std::sort(results.summary.modifyHeadersActions.begin(), results.summary.modifyHeadersActions.end(),
        [] (const ModifyHeadersAction& a, const ModifyHeadersAction& b) {
        return a.priority > b.priority;
    });

    HashMap<String, ModifyHeadersAction::ModifyHeadersOperationType> headerNameToFirstOperationApplied;
    for (auto& action : results.summary.modifyHeadersActions)
        action.applyToRequest(request, headerNameToFirstOperationApplied);

    for (auto& pair : results.summary.redirectActions)
        pair.first.applyToRequest(request, pair.second);

    if (page && results.shouldNotifyApplication()) {
        results.results.removeAllMatching([](const auto& pair) {
            return !pair.second.shouldNotifyApplication();
        });
        page->chrome().client().contentRuleListNotification(request.url(), results);
    }
}
    
} // namespace WebCore::ContentExtensions

#endif // ENABLE(CONTENT_EXTENSIONS)
