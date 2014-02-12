/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebPageProxy.h"

#import <WebCore/SerializedCryptoKeyWrap.h>

using namespace WebCore;

namespace WebKit {

#if ENABLE(SUBTLE_CRYPTO)
void WebPageProxy::wrapCryptoKey(const Vector<uint8_t>& key, bool& succeeded, Vector<uint8_t>& wrappedKey)
{
    Vector<uint8_t> masterKey(16);
    memset(masterKey.data(), 0, masterKey.size()); // FIXME: Not implemented yet, will be getting a key from client.
    succeeded = wrapSerializedCryptoKey(masterKey, key, wrappedKey);
}

void WebPageProxy::unwrapCryptoKey(const Vector<uint8_t>& wrappedKey, bool& succeeded, Vector<uint8_t>& key)
{
    Vector<uint8_t> masterKey(16);
    memset(masterKey.data(), 0, masterKey.size()); // FIXME: Not implemented yet, will be getting a key from client.
    succeeded = unwrapSerializedCryptoKey(masterKey, wrappedKey, key);
}
#endif

} // namespace WebKit
