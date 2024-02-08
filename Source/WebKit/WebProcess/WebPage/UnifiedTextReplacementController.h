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

#pragma once

#if ENABLE(UNIFIED_TEXT_REPLACEMENT)

#include "WebTextReplacementData.h"

#include <WebCore/Range.h>
#include <wtf/FastMalloc.h>
#include <wtf/Noncopyable.h>
#include <wtf/UUID.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class WebPage;

class UnifiedTextReplacementController final {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(UnifiedTextReplacementController);

public:
    explicit UnifiedTextReplacementController(WebPage&);

    void willBeginTextReplacementSession(const WTF::UUID&, CompletionHandler<void(const Vector<WebKit::WebUnifiedTextReplacementContextData>&)>&&);

    void didBeginTextReplacementSession(const WTF::UUID&, const Vector<WebKit::WebUnifiedTextReplacementContextData>&);

    void textReplacementSessionDidReceiveReplacements(const WTF::UUID&, const Vector<WebKit::WebTextReplacementData>&, const WebKit::WebUnifiedTextReplacementContextData&, bool finished);

    void textReplacementSessionDidUpdateStateForReplacement(const WTF::UUID&, WebKit::WebTextReplacementData::State, const WebKit::WebTextReplacementData&, const WebKit::WebUnifiedTextReplacementContextData&);

    void didEndTextReplacementSession(const WTF::UUID&, bool accepted);

private:
    WeakPtr<WebPage> m_webPage;

    HashMap<WTF::UUID, Ref<WebCore::Range>> m_contextRanges;
};

} // namespace WebKit

#endif
