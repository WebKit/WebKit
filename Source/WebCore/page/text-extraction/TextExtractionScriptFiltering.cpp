/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "TextExtractionScriptFiltering.h"

#include "CommonVM.h"
#include "DOMWrapperWorld.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "DocumentWriter.h"
#include "EmptyClients.h"
#include "FrameLoader.h"
#include "LocalFrameInlines.h"
#include "LocalFrameView.h"
#include "Page.h"
#include "PageConfiguration.h"
#include "RunJavaScriptParameters.h"
#include "SandboxFlags.h"
#include "ScriptController.h"
#include "Settings.h"
#include "TextExtractionTypes.h"
#include "markup.h"
#include <JavaScriptCore/JSCInlines.h>
#include <pal/SessionID.h>
#include <wtf/Box.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/MainThread.h>
#include <wtf/URL.h>

namespace WebCore::TextExtraction {

using namespace JSC;

Ref<Page> createScriptFilteringPage()
{
    ASSERT(isMainThread());

    auto configuration = pageConfigurationWithEmptyClients(std::nullopt, PAL::SessionID::defaultSessionID());
    configuration.allowedNetworkHosts = MemoryCompactLookupOnlyRobinHoodHashSet<String> { };

    if (auto* mainFrameParameters = std::get_if<PageConfiguration::LocalMainFrameCreationParameters>(&configuration.mainFrameCreationParameters))
        mainFrameParameters->effectiveSandboxFlags.remove(SandboxFlag::Scripts);

    Ref page = createPageForSanitizingWebContent(nullptr, { WTF::move(configuration) });
    page->settings().setScriptEnabled(true);
    return page;
}

void applyScriptFilteringRules(const String& input, const URL& documentURL, const Vector<FilterRule>& rules, Page& filterPage, CompletionHandler<void(const String&)>&& completion)
{
    ASSERT(isMainThread());

    if (rules.isEmpty())
        return completion(input);

    RefPtr frame = filterPage.localMainFrame();
    RefPtr document = frame ? frame->document() : nullptr;
    if (!frame || !document)
        return completion(input);

    auto filteredStrings = Box<Vector<String>>::create();
    auto aggregator = MainRunLoopCallbackAggregator::create([completion = WTF::move(completion), input, filteredStrings] mutable {
        if (filteredStrings->isEmpty())
            return completion(input);

        auto shortestFilteredString = std::ranges::min(*filteredStrings, { }, [](auto& string) {
            return string.length();
        });
        completion(WTF::move(shortestFilteredString));
    });

    auto urlString = documentURL.string();
    Ref world = mainThreadNormalWorldSingleton();

    for (auto& [name, urlPattern, source] : rules) {
        bool shouldApplyRule = WTF::switchOn(urlPattern, [](FilterRulePattern pattern) {
            return pattern == FilterRulePattern::Global;
        }, [&](const Yarr::RegularExpression& regex) {
            return regex.match(urlString) >= 0;
        });

        if (!shouldApplyRule)
            continue;

        ArgumentMap argumentMap;
        argumentMap.reserveInitialCapacity(1);
        argumentMap.add("input"_s, [input](auto& lexicalGlobalObject) {
            JSLockHolder lock { &lexicalGlobalObject };
            return JSValue { jsString(commonVM(), input) };
        });

        RunJavaScriptParameters parameters {
            source,
            SourceTaintedOrigin::Untainted,
            { },
            true, // runAsAsyncFunction
            std::make_optional(WTF::move(argumentMap)),
            false, // forceUserGesture
            RemoveTransientActivation::No
        };

        JSLockHolder lock(commonVM());
        protect(frame->script())->executeAsynchronousUserAgentScriptInWorld(world, WTF::move(parameters), [document, aggregator, filteredStrings](auto valueOrException) {
            if (!valueOrException)
                return;

            auto jsValue = valueOrException.value();
            if (!jsValue.isString())
                return;

            filteredStrings->append(jsValue.getString(document->globalObject()));
        });
    }
}

} // namespace WebCore::TextExtraction
