/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionDynamicScripts.h"

#if ENABLE(WK_WEB_EXTENSIONS)
#import "APIData.h"
#import "APIError.h"
#import "CocoaHelpers.h"
#import "WKContentWorld.h"
#import "WKFrameInfoPrivate.h"
#import "WKNSData.h"
#import "WKNSError.h"
#import "WKWebViewInternal.h"
#import "WKWebViewPrivate.h"
#import "WebExtension.h"
#import "WebExtensionContext.h"
#import "WebExtensionFrameIdentifier.h"
#import "WebExtensionScriptInjectionParameters.h"
#import "WebExtensionUtilities.h"
#import "WebPageProxy.h"
#import "WebUserContentControllerProxy.h"
#import "_WKFrameHandle.h"
#import "_WKFrameTreeNode.h"
#import <wtf/CallbackAggregator.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/URL.h>
#import <wtf/cocoa/VectorCocoa.h>

namespace WebKit {

namespace WebExtensionDynamicScripts {

static bool userStyleSheetMatchesContent(Ref<API::UserStyleSheet> userStyleSheet, SourcePair styleSheetContent, WebCore::UserContentInjectedFrames injectedFrames)
{
    return userStyleSheet->userStyleSheet().source() == styleSheetContent.first && userStyleSheet->userStyleSheet().injectedFrames() == injectedFrames;
}

Vector<RetainPtr<_WKFrameTreeNode>> getFrames(_WKFrameTreeNode *currentNode, std::optional<Vector<WebExtensionFrameIdentifier>> frameIDs)
{
    Vector<RetainPtr<_WKFrameTreeNode>> matchingFrames;
    Vector<RetainPtr<_WKFrameTreeNode>> framesToCheck { currentNode };

    while (!framesToCheck.isEmpty()) {
        _WKFrameTreeNode *frame = framesToCheck.first().get();
        framesToCheck.removeFirst(frame);

        auto currentFrameID = toWebExtensionFrameIdentifier(frame.info);
        if (!frameIDs || frameIDs->contains(currentFrameID))
            matchingFrames.append(frame);

        for (_WKFrameTreeNode *child in frame.childFrames)
            framesToCheck.append(child);
    }

    return matchingFrames;
}

std::optional<SourcePair> sourcePairForResource(String path, WebExtensionContext& extensionContext)
{
    RefPtr<API::Error> error;
    RefPtr scriptData = extensionContext.protectedExtension()->resourceDataForPath(path, error);
    if (!scriptData || error) {
        extensionContext.recordError(wrapper(error));
        return std::nullopt;
    }

    return SourcePair { String::fromUTF8(scriptData->span()), { extensionContext.baseURL(), path } };
}

SourcePairs getSourcePairsForParameters(const WebExtensionScriptInjectionParameters& parameters, WebExtensionContext& extensionContext)
{
    if (parameters.files) {
        return WTF::compactMap(parameters.files.value(), [&](auto& file) -> std::optional<SourcePair> {
            return sourcePairForResource(file, extensionContext);
        });
    }

    return { SourcePair { parameters.code.value_or(parameters.function.value_or(parameters.css.value_or(""_s))), URL { } } };
}

class InjectionResultHolder : public RefCounted<InjectionResultHolder> {
    WTF_MAKE_NONCOPYABLE(InjectionResultHolder);
    WTF_MAKE_TZONE_ALLOCATED_INLINE(InjectionResultHolder);

public:
    template<typename... Args>
    static Ref<InjectionResultHolder> create(Args&&... args)
    {
        return adoptRef(*new InjectionResultHolder(std::forward<Args>(args)...));
    }

    InjectionResultHolder() = default;

    InjectionResults results;
};

void executeScript(const SourcePairs& scriptPairs, WKWebView *webView, API::ContentWorld& executionWorld, WebExtensionTab& tab, const WebExtensionScriptInjectionParameters& parameters, WebExtensionContext& context, CompletionHandler<void(InjectionResults&&)>&& completionHandler)
{
    auto injectionResults = InjectionResultHolder::create();
    auto aggregator = MainRunLoopCallbackAggregator::create([injectionResults, completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler(WTFMove(injectionResults->results));
    });

    [webView _frames:makeBlockPtr([webView = RetainPtr { webView }, tab = Ref { tab }, context = Ref { context }, scriptPairs, executionWorld = Ref { executionWorld }, injectionResults, aggregator, parameters](_WKFrameTreeNode *mainFrame) mutable {
        if (!mainFrame.info.isMainFrame) {
            RELEASE_LOG_INFO(Extensions, "Not executing script because the mainFrame is nil");
            injectionResults->results.append(toInjectionResultParameters(nil, nil, @"Failed to execute script."));
            return;
        }

        WKContentWorld *world = executionWorld->wrapper();
        Vector<RetainPtr<_WKFrameTreeNode>> frames = getFrames(mainFrame, parameters.frameIDs);

        for (auto& frame : frames) {
            WKFrameInfo *frameInfo = frame.get().info;
            NSURL *frameURL = frameInfo.request.URL;

            if (!context->hasPermission(frameURL, tab.ptr())) {
                injectionResults->results.append(toInjectionResultParameters(nil, frameInfo, @"Failed to execute script. Extension does not have access to this frame."));
                continue;
            }

            if (parameters.function) {
                NSString *javaScript = [NSString stringWithFormat:@"return (%@)(...arguments)", (NSString *)parameters.function.value()];
                NSArray *arguments = parameters.arguments ? parseJSON(parameters.arguments.value(), JSONOptions::FragmentsAllowed) : @[ ];

                [webView _callAsyncJavaScript:javaScript arguments:@{ @"arguments": arguments } inFrame:frameInfo inContentWorld:world completionHandler:makeBlockPtr([injectionResults, aggregator, frameInfo](id resultOfExecution, NSError *error) mutable {
                    injectionResults->results.append(toInjectionResultParameters(resultOfExecution, frameInfo, error.localizedDescription));
                }).get()];

                continue;
            }

            for (auto& script : scriptPairs) {
                [webView _evaluateJavaScript:script.first withSourceURL:script.second inFrame:frameInfo inContentWorld:executionWorld->wrapper() completionHandler:makeBlockPtr([injectionResults, aggregator, frameInfo](id resultOfExecution, NSError *error) mutable {
                    injectionResults->results.append(toInjectionResultParameters(resultOfExecution, frameInfo, error.localizedDescription));
                }).get()];
            }
        }
    }).get()];
}

void injectStyleSheets(const SourcePairs& styleSheetPairs, WKWebView *webView, API::ContentWorld& executionWorld, WebCore::UserStyleLevel styleLevel, WebCore::UserContentInjectedFrames injectedFrames, WebExtensionContext& context)
{
    auto page = webView._page;
    auto pageID = page->webPageIDInMainFrameProcess();

    for (auto& styleSheet : styleSheetPairs) {
        auto userStyleSheet = API::UserStyleSheet::create(WebCore::UserStyleSheet { styleSheet.first, styleSheet.second, Vector<String> { }, Vector<String> { }, injectedFrames, styleLevel, pageID }, executionWorld);

        auto& controller = page.get()->userContentController();
        controller.addUserStyleSheet(userStyleSheet);

        context.dynamicallyInjectedUserStyleSheets().append(userStyleSheet);
    }
}

void removeStyleSheets(const SourcePairs& styleSheetPairs, WKWebView *webView,  WebCore::UserContentInjectedFrames injectedFrames, WebExtensionContext& context)
{
    UserStyleSheetVector styleSheetsToRemove;
    auto& dynamicallyInjectedUserStyleSheets = context.dynamicallyInjectedUserStyleSheets();

    for (auto& styleSheetContent : styleSheetPairs) {
        for (auto& userStyleSheet : dynamicallyInjectedUserStyleSheets) {
            if (userStyleSheetMatchesContent(userStyleSheet, styleSheetContent, injectedFrames)) {
                styleSheetsToRemove.append(userStyleSheet);
                auto& controller = webView._page.get()->userContentController();
                controller.removeUserStyleSheet(userStyleSheet);
            }
        }

        for (auto& stylesheet : styleSheetsToRemove)
            dynamicallyInjectedUserStyleSheets.removeFirst(stylesheet);

        styleSheetsToRemove.clear();
    }
}

WebExtensionScriptInjectionResultParameters toInjectionResultParameters(id resultOfExecution, WKFrameInfo *info, NSString *errorMessage)
{
    WebExtensionScriptInjectionResultParameters parameters;

    if (resultOfExecution)
        parameters.resultJSON = encodeJSONString(resultOfExecution, JSONOptions::FragmentsAllowed);

    if (info)
        parameters.frameID = toWebExtensionFrameIdentifier(info);

    if (errorMessage)
        parameters.error = errorMessage;

    return parameters;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebExtensionRegisteredScript);

void WebExtensionRegisteredScript::updateParameters(const WebExtensionRegisteredScriptParameters& parameters)
{
    m_parameters = parameters;
}

void WebExtensionRegisteredScript::merge(WebExtensionRegisteredScriptParameters& parameters)
{
    if (!parameters.css && m_parameters.css)
        parameters.css = m_parameters.css.value();

    if (!parameters.js && m_parameters.js)
        parameters.js = m_parameters.js.value();

    if (!parameters.injectionTime)
        parameters.injectionTime = m_parameters.injectionTime.value();

    if (!parameters.excludeMatchPatterns && m_parameters.excludeMatchPatterns)
        parameters.excludeMatchPatterns = m_parameters.excludeMatchPatterns.value();

    if (!parameters.matchPatterns)
        parameters.matchPatterns = m_parameters.matchPatterns.value();

    if (!parameters.allFrames)
        parameters.allFrames = m_parameters.allFrames.value();

    if (!parameters.persistent)
        parameters.persistent = m_parameters.persistent.value();

    if (!parameters.world)
        parameters.world = m_parameters.world.value();
}

void WebExtensionRegisteredScript::addUserScript(const String& identifier, API::UserScript& userScript)
{
    auto& userScripts = m_userScriptsMap.ensure(identifier, [&] {
        return UserScriptVector { };
    }).iterator->value;
    userScripts.append(userScript);
}

void WebExtensionRegisteredScript::addUserStyleSheet(const String& identifier, API::UserStyleSheet& userStyleSheet)
{
    auto& userStyleSheets = m_userStyleSheetsMap.ensure(identifier, [&] {
        return UserStyleSheetVector { };
    }).iterator->value;
    userStyleSheets.append(userStyleSheet);
}

void WebExtensionRegisteredScript::removeUserScriptsAndStyleSheets(const String& identifier)
{
    removeUserScripts(identifier);
    removeUserStyleSheets(identifier);
}

void WebExtensionRegisteredScript::removeUserScripts(const String& identifier)
{
    auto userScripts = m_userScriptsMap.take(identifier);
    auto allUserContentControllers = m_extensionContext->extensionController()->allUserContentControllers();

    for (auto& userScript : userScripts) {
        for (auto& userContentController : allUserContentControllers)
            userContentController.removeUserScript(userScript);
    }
}

void WebExtensionRegisteredScript::removeUserStyleSheets(const String& identifier)
{
    auto userStyleSheets = m_userStyleSheetsMap.take(identifier);
    auto allUserContentControllers = m_extensionContext->extensionController()->allUserContentControllers();

    for (auto& userStyleSheet : userStyleSheets) {
        for (auto& userContentController : allUserContentControllers)
            userContentController.removeUserStyleSheet(userStyleSheet);
    }
}

} // namespace WebExtensionDynamicScripts

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
