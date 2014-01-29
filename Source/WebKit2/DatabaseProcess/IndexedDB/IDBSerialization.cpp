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
#include "config.h"
#include "IDBSerialization.h"

#if ENABLE(INDEXED_DATABASE)

#include "ArgumentEncoder.h"
#include "KeyedDecoder.h"
#include "KeyedEncoder.h"
#include <WebCore/IDBKeyData.h>
#include <WebCore/IDBKeyPath.h>

using namespace WebCore;

namespace WebKit {

RefPtr<SharedBuffer> serializeIDBKeyPath(const IDBKeyPath& keyPath)
{
    KeyedEncoder encoder;
    keyPath.encode(encoder);
    return encoder.finishEncoding();
}

bool deserializeIDBKeyPath(const uint8_t* data, size_t size, IDBKeyPath& result)
{
    KeyedDecoder decoder(data, size);
    return IDBKeyPath::decode(decoder, result);
}

RefPtr<WebCore::SharedBuffer> serializeIDBKeyData(const IDBKeyData& key)
{
    KeyedEncoder encoder;
    key.encode(encoder);
    return encoder.finishEncoding();
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE)
