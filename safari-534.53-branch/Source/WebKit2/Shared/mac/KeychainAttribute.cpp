/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "KeychainAttribute.h"

#include "ArgumentCoders.h"
#include "ArgumentCodersCF.h"

namespace WebKit {

KeychainAttribute::KeychainAttribute()
    : tag(0)
{
}

KeychainAttribute::KeychainAttribute(const SecKeychainAttribute& secKeychainAttribute)
    : tag(secKeychainAttribute.tag)
{
    if (!secKeychainAttribute.data)
        return;
    
    data.adoptCF(CFDataCreate(0, static_cast<UInt8*>(secKeychainAttribute.data), secKeychainAttribute.length));
}

} // namespace WebKit

namespace CoreIPC {

void encode(CoreIPC::ArgumentEncoder* encoder, const WebKit::KeychainAttribute& attribute)
{
    encoder->encodeUInt32(static_cast<uint32_t>(attribute.tag));
    encoder->encodeBool(attribute.data);
    if (attribute.data)
        CoreIPC::encode(encoder, attribute.data.get());
}

bool decode(CoreIPC::ArgumentDecoder* decoder, WebKit::KeychainAttribute& attribute)
{
    uint32_t tag;
    if (!decoder->decodeUInt32(tag))
        return false;

    attribute.tag = tag;

    bool expectData;
    if (!decoder->decodeBool(expectData))
        return false;
    
    if (expectData && !CoreIPC::decode(decoder, attribute.data))
        return false;
    
    return true;
}

} // namespace CoreIPC
