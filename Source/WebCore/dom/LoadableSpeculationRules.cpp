/*
 * Copyright (C) 2025 Shopify Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LoadableSpeculationRules.h"

#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "CachedScript.h"
#include "CrossOriginAccessControl.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "DocumentResourceLoader.h"
#include "FrameDestructionObserverInlines.h"
#include "LocalFrame.h"
#include "ResourceRequest.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"

#include <wtf/text/MakeString.h>
#include <wtf/text/StringConcatenate.h>

namespace WebCore {

Ref<LoadableSpeculationRules> LoadableSpeculationRules::create(Document& document, const URL& url)
{
    return adoptRef(*new LoadableSpeculationRules(document, url));
}

LoadableSpeculationRules::LoadableSpeculationRules(Document& document, const URL& url)
    : m_document(document)
    , m_url(url)
{
}

LoadableSpeculationRules::~LoadableSpeculationRules()
{
    if (m_cachedScript)
        m_cachedScript->removeClient(*this);
}

CachedResourceHandle<CachedScript> LoadableSpeculationRules::requestSpeculationRules(Document& document, const URL& sourceURL)
{
    // https://html.spec.whatwg.org/C#the-speculation-rules-header
    // 3.4.2.2.1. Let request be a new request whose URL is url, destination is "speculationrules", and mode is "cors".
    if (!document.settings().isScriptEnabled())
        return nullptr;

    ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();
    options.contentSecurityPolicyImposition = ContentSecurityPolicyImposition::DoPolicyCheck;
    options.sameOriginDataURLFlag = SameOriginDataURLFlag::Set;
    options.serviceWorkersMode = ServiceWorkersMode::All;
    options.integrity = ""_s;
    options.referrerPolicy = ReferrerPolicy::EmptyString;
    options.fetchPriority = RequestPriority::Auto;
    options.destination = FetchOptionsDestination::Speculationrules;

    auto request = createPotentialAccessControlRequest(URL { sourceURL }, WTF::move(options), document, ""_s);
    request.upgradeInsecureRequestIfNeeded(document);
    request.setPriority(ResourceLoadPriority::Low);

    return protect(document.cachedResourceLoader())->requestScript(WTF::move(request)).value_or(nullptr);
}

bool LoadableSpeculationRules::load(Document& document, const URL& url)
{
    ASSERT(!m_cachedScript);

    if (!url.isValid())
        return false;

    CachedResourceHandle cachedScript = requestSpeculationRules(document, m_url);
    m_cachedScript = cachedScript;
    if (!cachedScript)
        return false;
    cachedScript->addClient(*this);

    return true;
}

// https://html.spec.whatwg.org/C#process-the-speculation-rules-header
// 3.4.2.2. processResponseConsumeBody
void LoadableSpeculationRules::notifyFinished(CachedResource& resource, const NetworkLoadMetrics&, LoadWillContinueInAnotherProcess)
{
    ASSERT(&resource == m_cachedScript.get());

    RefPtr document = m_document.get();
    if (!document)
        return;

    // 1. If bodyBytes is null or failure, then abort these steps.
    // 2. If response's status is not an ok status, then abort these steps.
    if (m_cachedScript->errorOccurred()) {
        document->addConsoleMessage(MessageSource::Other, MessageLevel::Error, makeString("Failed to load speculation rules from "_s, m_url.string()));
        return;
    }

    // 3. If the result of extracting a MIME type from response's header list does not have an essence of "application/speculationrules+json", then abort these steps.
    if (resource.mimeType() != "application/speculationrules+json") {
        document->addConsoleMessage(MessageSource::Other, MessageLevel::Error, makeString("Invalid speculation rules MIME type "_s, m_url.string()));
        return;
    }

    // 4. Let bodyText be the result of UTF-8 decoding bodyBytes.
    String speculationRulesText = m_cachedScript->script(CachedScript::ShouldDecodeAsUTF8Only::Yes).toString();
    if (speculationRulesText.isEmpty())
        return;

    if (RefPtr frame = document->frame()) {
        ScriptSourceCode sourceCode(speculationRulesText, JSC::SourceTaintedOrigin::Untainted, URL(m_url), TextPosition(), JSC::SourceProviderSourceType::Program);
        // 5. Let ruleSet be the result of parsing a speculation rule set string given bodyText, document, and response's URL. If this throws an exception, then abort these steps.
        // 6. Append ruleSet to document's speculation rule sets.
        // Header-based rules use the Document as the source node.
        if (frame->checkedScript()->registerSpeculationRules(*document, sourceCode, m_url)) {
            // 7. Consider speculative loads for document.
            document->considerSpeculationRules();
        }
    }
}

} // namespace WebCore
