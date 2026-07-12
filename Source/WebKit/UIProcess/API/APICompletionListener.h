/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "APIObject.h"
#include "WKBase.h"
#include <wtf/CompletionHandler.h>

namespace API {

class CompletionListener : public API::ObjectImpl<API::Object::Type::CompletionListener> {
public:
    // The handler is stored for later dispatch via complete(), so CompletionListener is a
    // genuine leaf. Its member is enforced: complete() now proves at compile time that the
    // stored handler is invoked (token discarded at the single drain site). The single
    // deferUnchecked that produces the caller's token lives at the storage site in WKPage.cpp
    // that moves the outer handler into this listener.
    //
    // A single non-enforced create() is kept (constructing the enforced member from a plain
    // CompletionHandler is free) to avoid overload ambiguity for token-returning lambdas,
    // which are convertible to both enforced and non-enforced CompletionHandler.
    static Ref<CompletionListener> create(CompletionHandler<void(WKTypeRef)>&& completionHandler) { return adoptRef(*new CompletionListener(CompletionHandler<void(WKTypeRef), true>(WTF::move(completionHandler)))); }

    void complete(WKTypeRef result) { (void)m_completionHandler(result); }

private:
    explicit CompletionListener(CompletionHandler<void(WKTypeRef), true>&& completionHandler)
        : m_completionHandler(WTF::move(completionHandler)) { }

    CompletionHandler<void(WKTypeRef), true> m_completionHandler;
};

}

SPECIALIZE_TYPE_TRAITS_API_OBJECT(CompletionListener);
