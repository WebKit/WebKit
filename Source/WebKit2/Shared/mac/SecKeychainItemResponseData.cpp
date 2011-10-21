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
#include "SecKeychainItemResponseData.h"

#include "ArgumentCoders.h"
#include "ArgumentCodersCF.h"

namespace WebKit {

SecKeychainItemResponseData::SecKeychainItemResponseData()
    : m_itemClass(0)
    , m_resultCode(0)
{
}

SecKeychainItemResponseData::SecKeychainItemResponseData(OSStatus resultCode, SecItemClass itemClass, SecKeychainAttributeList* attrList, RetainPtr<CFDataRef> data)
    : m_itemClass(itemClass)
    , m_data(data)
    , m_resultCode(resultCode)
{
    if (!attrList)
        return;
    
    m_attributes.reserveCapacity(attrList->count);
    for (size_t i = 0; i < attrList->count; ++i)
        m_attributes.append(KeychainAttribute(attrList->attr[i]));
}

SecKeychainItemResponseData::SecKeychainItemResponseData(OSStatus resultCode, RetainPtr<SecKeychainItemRef> keychainItem)
    : m_resultCode(resultCode)
    , m_keychainItem(keychainItem)
{
}

SecKeychainItemResponseData::SecKeychainItemResponseData(OSStatus resultCode)
    : m_resultCode(resultCode)
{
}

void SecKeychainItemResponseData::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    encoder->encodeInt64((int64_t)m_resultCode);
    encoder->encodeUInt32(m_itemClass);
    encoder->encodeUInt32(m_attributes.size());
    for (size_t i = 0, count = m_attributes.size(); i < count; ++i)
        CoreIPC::encode(encoder, m_attributes[i]);

    encoder->encodeBool(m_data.get());
    if (m_data)
        CoreIPC::encode(encoder, m_data.get());

    encoder->encodeBool(m_keychainItem.get());
    if (m_keychainItem)
        CoreIPC::encode(encoder, m_keychainItem.get());
}

bool SecKeychainItemResponseData::decode(CoreIPC::ArgumentDecoder* decoder, SecKeychainItemResponseData& secKeychainItemResponseData)
{
    int64_t resultCode;
    if (!decoder->decodeInt64(resultCode))
        return false;
    uint32_t itemClass;
    if (!decoder->decodeUInt32(itemClass))
        return false;
    uint32_t attributeCount;
    if (!decoder->decodeUInt32(attributeCount))
        return false;

    secKeychainItemResponseData.m_resultCode = (OSStatus)resultCode;
    secKeychainItemResponseData.m_itemClass = (SecItemClass)itemClass;

    ASSERT(secKeychainItemResponseData.m_attributes.isEmpty());
    secKeychainItemResponseData.m_attributes.reserveCapacity(attributeCount);
    for (size_t i = 0; i < attributeCount; ++i) {
        KeychainAttribute attribute;
        if (!CoreIPC::decode(decoder, attribute))
            return false;
        secKeychainItemResponseData.m_attributes.append(attribute);
    }

    bool expectData;
    if (!decoder->decodeBool(expectData))
        return false;

    if (expectData && !CoreIPC::decode(decoder, secKeychainItemResponseData.m_data))
        return false;

    bool expectItem;
    if (!decoder->decodeBool(expectItem))
        return false;

    if (expectItem && !CoreIPC::decode(decoder, secKeychainItemResponseData.m_keychainItem))
        return false;

    return true;
}

} // namespace WebKit
