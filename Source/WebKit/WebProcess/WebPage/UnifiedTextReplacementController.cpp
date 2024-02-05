/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE(UNIFIED_TEXT_REPLACEMENT)

#include "config.h"
#include "UnifiedTextReplacementController.h"

#include "Logging.h"
#include "WebPage.h"
#include "WebUnifiedTextReplacementContextData.h"
#include <WebCore/DocumentMarkerController.h>
#include <WebCore/HTMLConverter.h>
#include <WebCore/TextIterator.h>

namespace WebKit {
using namespace WebCore;

UnifiedTextReplacementController::UnifiedTextReplacementController(WebPage& webPage)
    : m_webPage(webPage)
{
}

void UnifiedTextReplacementController::willBeginTextReplacementSession(const WTF::UUID& uuid, CompletionHandler<void(const Vector<WebUnifiedTextReplacementContextData>&)>&& completionHandler)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::willBeginTextReplacementSession (%s)", uuid.toString().utf8().data());

    if (!m_webPage)
        return;

    auto* corePage = m_webPage->corePage();
    if (!corePage)
        return;

    Ref frame = CheckedRef(corePage->focusController())->focusedOrMainFrame();

    auto contextRange = m_webPage->autocorrectionContextRange();
    if (!contextRange)
        return;

    auto liveRange = createLiveRange(*contextRange);

    ASSERT(!m_contextRanges.contains(uuid));

    m_contextRanges.set(uuid, liveRange);

    auto selectedTextRange = frame->selection().selection().firstRange();

    auto attributedStringFromRange = attributedString(*contextRange);

    auto selectedTextCharacterRange = WebCore::characterRange(*contextRange, *selectedTextRange);

    completionHandler({ WebUnifiedTextReplacementContextData { WTF::UUID { 0 }, attributedStringFromRange, selectedTextCharacterRange } });
}

void UnifiedTextReplacementController::didBeginTextReplacementSession(const WTF::UUID& uuid, const Vector<WebKit::WebUnifiedTextReplacementContextData>& contexts)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::didBeginTextReplacementSession (%s) [received contexts: %zu]", uuid.toString().utf8().data(), contexts.size());
}

void UnifiedTextReplacementController::textReplacementSessionDidReceiveReplacements(const WTF::UUID& uuid, const Vector<WebTextReplacementData>& replacements, const WebUnifiedTextReplacementContextData& context, bool finished)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveReplacements (%s) [received replacements: %zu, finished: %d]", uuid.toString().utf8().data(), replacements.size(), finished);

    if (!m_webPage)
        return;

    ASSERT(m_contextRanges.contains(uuid));

    auto liveRange = m_contextRanges.get(uuid);
    auto sessionRange = makeSimpleRange(liveRange);
    if (!sessionRange)
        return;

    for (const auto& replacement : replacements) {
        auto resolvedRange = resolveCharacterRange(*sessionRange, replacement.originalRange);

        addMarker(resolvedRange, DocumentMarker::Type::UnifiedTextReplacement, replacement.originalString.string);
    }
}

} // namespace WebKit

#endif
