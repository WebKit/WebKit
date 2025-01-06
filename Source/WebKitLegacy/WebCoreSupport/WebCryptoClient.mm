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

#import "WebCryptoClient.h"

#import "WebDelegateImplementationCaching.h"
#import "WebFramePrivate.h"
#import "WebUIDelegatePrivate.h"
#import <WebCore/CryptoKey.h>
#import <WebCore/SerializedCryptoKeyWrap.h>
#import <WebCore/SerializedScriptValue.h>
#import <WebCore/WrappedCryptoKey.h>
#import <optional>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cocoa/VectorCocoa.h>

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebCryptoClient);

std::optional<Vector<uint8_t>> WebCryptoClient::wrapCryptoKey(const Vector<uint8_t>& key) const
{
    SEL selector = @selector(webCryptoMasterKeyForWebView:);
    Vector<uint8_t> wrappedKey;
    if ([[m_webView UIDelegate] respondsToSelector:selector]) {
        auto masterKey = makeVector(CallUIDelegate(m_webView, selector));
        if (!WebCore::wrapSerializedCryptoKey(masterKey, key, wrappedKey))
            return std::nullopt;
        return wrappedKey;
    }

    auto masterKey = WebCore::defaultWebCryptoMasterKey();
    if (!masterKey)
        return std::nullopt;
    if (!WebCore::wrapSerializedCryptoKey(WTFMove(*masterKey), key, wrappedKey))
        return std::nullopt;
    return wrappedKey;
}

std::optional<Vector<uint8_t>> WebCryptoClient::serializeAndWrapCryptoKey(WebCore::CryptoKeyData&& keyData) const
{
    auto key = WebCore::CryptoKey::create(WTFMove(keyData));
    if (!key)
        return std::nullopt;

    JSContextRef context = [[m_webView mainFrame] globalContext];
    auto serializedKey = WebCore::SerializedScriptValue::serializeCryptoKey(context, *key);
    return wrapCryptoKey(serializedKey);
}

std::optional<Vector<uint8_t>> WebCryptoClient::unwrapCryptoKey(const Vector<uint8_t>& serializedKey) const
{
    auto wrappedKey = WebCore::readSerializedCryptoKey(serializedKey);
    if (!wrappedKey)
        return std::nullopt;
    SEL selector = @selector(webCryptoMasterKeyForWebView:);
    if ([[m_webView UIDelegate] respondsToSelector:selector]) {
        auto masterKey = makeVector(CallUIDelegate(m_webView, selector));
        return WebCore::unwrapCryptoKey(masterKey, *wrappedKey);
    }
    if (auto masterKey = WebCore::defaultWebCryptoMasterKey())
        return WebCore::unwrapCryptoKey(*masterKey, *wrappedKey);
    return std::nullopt;
}

WebCryptoClient::WebCryptoClient(WebView* webView)
    : m_webView(webView)
{
}
