/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "ISOFairPlayStreamingPsshBox.h"

#include <JavaScriptCore/DataView.h>

namespace WebCore {

const Vector<uint8_t>& ISOFairPlayStreamingPsshBox::fairPlaySystemID()
{
    static NeverDestroyed<Vector<uint8_t>> systemID = Vector<uint8_t>({ 0x94, 0xCE, 0x86, 0xFB, 0x07, 0xFF, 0x4F, 0x43, 0xAD, 0xB8, 0x93, 0xD2, 0xFA, 0x96, 0x8C, 0xA2 });
    return systemID;
}

bool ISOFairPlayStreamingInfoBox::parse(JSC::DataView& view, unsigned& offset)
{
    if (!ISOFullBox::parse(view, offset))
        return false;

    return checkedRead<uint32_t>(m_scheme, view, offset, BigEndian);
}

bool ISOFairPlayStreamingKeyRequestInfoBox::parse(JSC::DataView& view, unsigned& offset)
{
    unsigned localOffset = offset;
    if (!ISOBox::parse(view, localOffset))
        return false;

    CheckedUint64 remaining = m_size;
    remaining -= (localOffset - offset);
    if (remaining.hasOverflowed())
        return false;

    if (remaining < m_keyID.capacity())
        return false;

    auto buffer = view.possiblySharedBuffer();
    if (!buffer)
        return false;

    auto keyID = buffer->slice(localOffset, localOffset + m_keyID.capacity());
    localOffset += m_keyID.capacity();

    m_keyID.resize(m_keyID.capacity());
    memcpy(m_keyID.data(), keyID->data(), m_keyID.capacity());

    offset = localOffset;
    return true;
}

bool ISOFairPlayStreamingKeyAssetIdBox::parse(JSC::DataView& view, unsigned& offset)
{
    unsigned localOffset = offset;
    if (!ISOBox::parse(view, localOffset))
        return false;

    if (localOffset - offset == m_size) {
        m_data.clear();
        offset = localOffset;
        return true;
    }

    auto buffer = view.possiblySharedBuffer();
    if (!buffer)
        return false;

    size_t dataSize;
    if (!WTF::safeSub(m_size, localOffset - offset, dataSize))
        return false;

    auto parsedData = buffer->slice(localOffset, localOffset + dataSize);
    localOffset += dataSize;

    m_data.resize(dataSize);
    memcpy(m_data.data(), parsedData->data(), dataSize);
    offset = localOffset;
    return true;
}

bool ISOFairPlayStreamingKeyContextBox::parse(JSC::DataView& view, unsigned& offset)
{
    unsigned localOffset = offset;
    if (!ISOBox::parse(view, localOffset))
        return false;

    if (localOffset - offset == m_size) {
        m_data.clear();
        offset = localOffset;
        return true;
    }

    auto buffer = view.possiblySharedBuffer();
    if (!buffer)
        return false;

    size_t dataSize;
    if (!WTF::safeSub(m_size, localOffset - offset, dataSize))
        return false;

    auto parsedData = buffer->slice(localOffset, localOffset + dataSize);
    localOffset += dataSize;

    m_data.resize(dataSize);
    memcpy(m_data.data(), parsedData->data(), dataSize);
    offset = localOffset;
    return true;
}

bool ISOFairPlayStreamingKeyVersionListBox::parse(JSC::DataView& view, unsigned& offset)
{
    unsigned localOffset = offset;
    if (!ISOBox::parse(view, localOffset))
        return false;

    do {
        if (localOffset - offset == m_size)
            break;

        uint64_t remaining;
        if (!WTF::safeSub(m_size, localOffset - offset, remaining))
            return false;

        if (remaining < sizeof(uint32_t))
            return false;

        uint32_t version;
        if (!checkedRead<uint32_t>(version, view, localOffset, BigEndian))
            return false;
        m_versions.append(version);
    } while (true);

    offset = localOffset;
    return true; 
}

bool ISOFairPlayStreamingKeyRequestBox::parse(JSC::DataView& view, unsigned& offset)
{
    unsigned localOffset = offset;
    if (!ISOBox::parse(view, localOffset))
        return false;

    if (!m_requestInfo.read(view, localOffset))
        return false;

    while (localOffset - offset < m_size) {
        auto result = peekBox(view, localOffset);
        if (!result)
            return false;

        auto name = result.value().first;
        if (name == ISOFairPlayStreamingKeyAssetIdBox::boxTypeName()) {
            if (m_assetID)
                return false;

            ISOFairPlayStreamingKeyAssetIdBox assetID;
            if (!assetID.read(view, localOffset))
                return false;

            m_assetID = WTFMove(assetID);
            continue;
        }

        if (name == ISOFairPlayStreamingKeyContextBox::boxTypeName()) {
            if (m_context)
                return false;

            ISOFairPlayStreamingKeyContextBox context;
            if (!context.read(view, localOffset))
                return false;

            m_context = WTFMove(context);
            continue;
        }

        if (name == ISOFairPlayStreamingKeyVersionListBox::boxTypeName()) {
            if (m_versionList)
                return false;

            ISOFairPlayStreamingKeyVersionListBox versionList;
            if (!versionList.read(view, localOffset))
                return false;

            m_versionList = WTFMove(versionList);
            continue;
        }

        // Unknown box type; error.
        return false;
    }   
    
    offset = localOffset;
    return true; 
}

bool ISOFairPlayStreamingInitDataBox::parse(JSC::DataView& view, unsigned& offset)
{
    unsigned localOffset = offset;
    if (!ISOBox::parse(view, localOffset))
        return false;

    if (!m_info.read(view, localOffset))
        return false;

    while (localOffset - offset < m_size) {
        ISOFairPlayStreamingKeyRequestBox request;
        if (!request.read(view, localOffset))
            return false;

        m_requests.append(WTFMove(request));
    }

    offset = localOffset;
    return true;
}

bool ISOFairPlayStreamingPsshBox::parse(JSC::DataView& view, unsigned& offset)
{
    if (!ISOProtectionSystemSpecificHeaderBox::parse(view, offset))
        return false;

    // Back up the offset by exactly the size of m_data:
    offset -= m_data.size();

    return m_initDataBox.read(view, offset);
}

}
