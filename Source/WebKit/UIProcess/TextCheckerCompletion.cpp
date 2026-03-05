/*
 * Copyright (C) 2013 Samsung Electronics
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TextCheckerCompletion.h"

#include "WebPageProxy.h"

namespace WebKit {
using namespace WebCore;

Ref<TextCheckerCompletion> TextCheckerCompletion::create(TextCheckerRequestID requestID, const TextCheckingRequestData& requestData, WebPageProxy& page)
{
    return adoptRef(*new TextCheckerCompletion(requestID, requestData, page));
}

TextCheckerCompletion::TextCheckerCompletion(TextCheckerRequestID requestID, const TextCheckingRequestData& requestData, WebPageProxy& page)
    : m_requestID(requestID)
    , m_requestData(requestData)
    , m_page(page)
{
}

const TextCheckingRequestData& TextCheckerCompletion::textCheckingRequestData() const
{
    return m_requestData;
}

int64_t TextCheckerCompletion::spellDocumentTag()
{
    RefPtr page = m_page.get();
    if (page)
        return page->spellDocumentTag();
    return -1;
}

void TextCheckerCompletion::didFinishCheckingText(const Vector<TextCheckingResult>& result) const
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    if (result.isEmpty())
        didCancelCheckingText();

    page->didFinishCheckingText(m_requestID, result);
}

void TextCheckerCompletion::didCancelCheckingText() const
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    page->didCancelCheckingText(m_requestID);
}

} // namespace WebKit
