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
#include "SecKeychainItemRequestData.h"

#if USE(SECURITY_FRAMEWORK)

#include "ArgumentCoders.h"
#include "ArgumentCodersCF.h"

namespace WebKit {

SecKeychainItemRequestData::SecKeychainItemRequestData()
    : m_type(Invalid)
    , m_itemClass(0)
    , m_attrs(adoptRef(new Attributes))
{
}

SecKeychainItemRequestData::SecKeychainItemRequestData(Type type, SecKeychainItemRef item, SecKeychainAttributeList* attrList)
    : m_type(type)
    , m_keychainItem(item)
    , m_itemClass(0)
    , m_attrs(adoptRef(new Attributes))
{
    initializeWithAttributeList(attrList);
}

SecKeychainItemRequestData::SecKeychainItemRequestData(Type type, SecKeychainItemRef item, SecKeychainAttributeList* attrList, UInt32 length, const void* data)
    : m_type(type)
    , m_keychainItem(item)
    , m_itemClass(0)
    , m_dataReference(static_cast<const uint8_t*>(data), length)
    , m_attrs(adoptRef(new Attributes))
{
    initializeWithAttributeList(attrList);
}

SecKeychainItemRequestData::SecKeychainItemRequestData(Type type, SecItemClass itemClass, SecKeychainAttributeList* attrList, UInt32 length, const void* data)
    : m_type(type)
    , m_itemClass(itemClass)
    , m_dataReference(static_cast<const uint8_t*>(data), length)
    , m_attrs(adoptRef(new Attributes))
{
    initializeWithAttributeList(attrList);
}

void SecKeychainItemRequestData::initializeWithAttributeList(SecKeychainAttributeList* attrList)
{
    if (!attrList)
        return;
    m_keychainAttributes.reserveCapacity(attrList->count);
    for (size_t i = 0; i < attrList->count; ++i)
        m_keychainAttributes.append(KeychainAttribute(attrList->attr[i]));
}

SecKeychainItemRequestData::~SecKeychainItemRequestData()
{
#ifndef NDEBUG
    if (!m_attrs->m_attributeList)
        return;

    // If this request was for SecKeychainItemModifyContent:
    //   - The data pointers should've been populated by this SecKeychainItemRequestData,
    //     and should match the data pointers contained in the corresponding CFDataRef
    // If this request was for SecKeychainItemCopyContent:
    //   - Security APIs should've filled in the data in the AttributeList and that data 
    //     should've been freed by SecKeychainItemFreeContent.
    for (size_t i = 0; i < m_attrs->m_attributeList->count; ++i) {
        if (m_keychainAttributes[i].data)
            ASSERT(m_attrs->m_attributeList->attr[i].data == CFDataGetBytePtr(m_keychainAttributes[i].data.get()));
        else
            ASSERT(!m_attrs->m_attributeList->attr[i].data);
    }
#endif
}

SecKeychainAttributeList* SecKeychainItemRequestData::attributeList() const
{
    if (m_attrs->m_attributeList || m_keychainAttributes.isEmpty())
        return m_attrs->m_attributeList.get();
    
    m_attrs->m_attributeList = adoptPtr(new SecKeychainAttributeList);
    m_attrs->m_attributeList->count = m_keychainAttributes.size();
    m_attrs->m_attributes = adoptArrayPtr(new SecKeychainAttribute[m_attrs->m_attributeList->count]);
    m_attrs->m_attributeList->attr = m_attrs->m_attributes.get();

    for (size_t i = 0; i < m_attrs->m_attributeList->count; ++i) {
        m_attrs->m_attributeList->attr[i].tag = m_keychainAttributes[i].tag;
        if (!m_keychainAttributes[i].data) {
            m_attrs->m_attributeList->attr[i].length = 0;
            m_attrs->m_attributeList->attr[i].data = 0;
            continue;
        }
        
        m_attrs->m_attributeList->attr[i].length = CFDataGetLength(m_keychainAttributes[i].data.get());
        m_attrs->m_attributeList->attr[i].data = const_cast<void*>(static_cast<const void*>(CFDataGetBytePtr(m_keychainAttributes[i].data.get())));
    }
    
    return m_attrs->m_attributeList.get();
}

void SecKeychainItemRequestData::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    encoder->encodeEnum(m_type);

    encoder->encodeBool(m_keychainItem);
    if (m_keychainItem)
        CoreIPC::encode(encoder, m_keychainItem.get());

    encoder->encodeUInt32(m_keychainAttributes.size());
    for (size_t i = 0, count = m_keychainAttributes.size(); i < count; ++i)
        CoreIPC::encode(encoder, m_keychainAttributes[i]);
    
    encoder->encodeUInt64(m_itemClass);
    m_dataReference.encode(encoder);
}

bool SecKeychainItemRequestData::decode(CoreIPC::ArgumentDecoder* decoder, SecKeychainItemRequestData& secKeychainItemRequestData)
{
    if (!decoder->decodeEnum(secKeychainItemRequestData.m_type))
        return false;

    bool hasKeychainItem;
    if (!decoder->decodeBool(hasKeychainItem))
        return false;

    if (hasKeychainItem && !CoreIPC::decode(decoder, secKeychainItemRequestData.m_keychainItem))
        return false;
    
    uint32_t attributeCount;
    if (!decoder->decodeUInt32(attributeCount))
        return false;
    
    ASSERT(secKeychainItemRequestData.m_keychainAttributes.isEmpty());
    secKeychainItemRequestData.m_keychainAttributes.reserveCapacity(attributeCount);
    
    for (size_t i = 0; i < attributeCount; ++i) {
        KeychainAttribute attribute;
        if (!CoreIPC::decode(decoder, attribute))
            return false;
        secKeychainItemRequestData.m_keychainAttributes.append(attribute);
    }
    
    uint64_t itemClass;
    if (!decoder->decodeUInt64(itemClass))
        return false;
    
    secKeychainItemRequestData.m_itemClass = static_cast<SecItemClass>(itemClass);

    if (!CoreIPC::DataReference::decode(decoder, secKeychainItemRequestData.m_dataReference))
        return false;
        
    return true;
}

} // namespace WebKit

#endif // USE(SECURITY_FRAMEWORK)
