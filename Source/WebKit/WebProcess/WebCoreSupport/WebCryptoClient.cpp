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
#include "WebCryptoClient.h"

#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
#include <WebCore/SerializedCryptoKeyWrap.h>
#include <WebCore/WrappedCryptoKey.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebCryptoClient);

std::optional<Vector<uint8_t>> WebCryptoClient::wrapCryptoKey(const Vector<uint8_t>& key) const
{
    if (m_pageIdentifier) {
        auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::WrapCryptoKey(key), *m_pageIdentifier);
        auto [wrappedKey] = sendResult.takeReplyOr(std::nullopt);
        return wrappedKey;
    }
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebProcessProxy::WrapCryptoKey(key), 0);

    auto [wrappedKey] = sendResult.takeReplyOr(std::nullopt);
    return wrappedKey;
}

std::optional<Vector<uint8_t>> WebCryptoClient::unwrapCryptoKey(const Vector<uint8_t>& wrappedKey) const
{
    auto deserializedKey = WebCore::readSerializedCryptoKey(wrappedKey);
    if (!deserializedKey)
        return std::nullopt;

    if (m_pageIdentifier) {
        auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::UnwrapCryptoKey(*deserializedKey), *m_pageIdentifier);
        auto [unwrappedKey] = sendResult.takeReplyOr(std::nullopt);
        return unwrappedKey;
    }

    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebProcessProxy::UnwrapCryptoKey(*deserializedKey), 0);
    auto [unwrappedKey] = sendResult.takeReplyOr(std::nullopt);
    return unwrappedKey;
}

WebCryptoClient::WebCryptoClient(WebCore::PageIdentifier pageIdentifier)
    : m_pageIdentifier(pageIdentifier)
{
}
}
