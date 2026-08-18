/*
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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
#include <wtf/CheckedArithmetic.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

const ISOFairPlayStreamingPsshBox::SystemID& ISOFairPlayStreamingPsshBox::fairPlaySystemID()
{
    static NeverDestroyed<SystemID> systemID { SystemID { { 0x94, 0xCE, 0x86, 0xFB, 0x07, 0xFF, 0x4F, 0x43, 0xAD, 0xB8, 0x93, 0xD2, 0xFA, 0x96, 0x8C, 0xA2 } } };
    return systemID;
}

bool ISOFairPlayStreamingInfoBox::parse(const ByteView& view, unsigned& offset)
{
    if (!ISOFullBox::parse(view, offset))
        return false;

    return checkedRead<uint32_t>(m_scheme, view, offset, BigEndian);
}

bool ISOFairPlayStreamingInfoBox::pack(MutableByteView& view, unsigned& offset) const
{
    if (!ISOFullBox::pack(view, offset))
        return false;

    return checkedWrite<uint32_t>(m_scheme.value, view, offset, BigEndian);
}

bool ISOFairPlayStreamingKeyRequestInfoBox::parse(const ByteView& view, unsigned& offset)
{
    unsigned localOffset = offset;
    if (!ISOFullBox::parse(view, localOffset))
        return false;

    CheckedUint64 remaining = m_size;
    remaining -= (localOffset - offset);
    if (remaining.hasOverflowed())
        return false;

    if (remaining < m_keyID.capacity())
        return false;

    m_keyID.resize(m_keyID.capacity());
    if (!checkedReadSequence<uint8_t>(m_keyID, view, localOffset, BigEndian))
        return false;

    offset = localOffset;
    return true;
}

bool ISOFairPlayStreamingKeyRequestInfoBox::pack(MutableByteView& view, unsigned& offset) const
{
    if (!ISOFullBox::pack(view, offset))
        return false;

    return checkedWriteSequence<uint8_t>(m_keyID, view, offset, BigEndian);
}

bool ISOFairPlayStreamingKeyAssetIdBox::parse(const ByteView& view, unsigned& offset)
{
    unsigned localOffset = offset;
    if (!ISOBox::parse(view, localOffset))
        return false;

    if (localOffset - offset == m_size) {
        m_data.clear();
        offset = localOffset;
        return true;
    }

    size_t dataSize;
    if (!WTF::safeSub(m_size, localOffset - offset, dataSize))
        return false;

    size_t remainingInView;
    if (!WTF::safeSub(view.size(), localOffset, remainingInView))
        return false;

    if (remainingInView < dataSize)
        return false;

    m_data.resize(dataSize);
    if (!checkedReadSequence<uint8_t>(m_data, view, localOffset, BigEndian))
        return false;

    offset = localOffset;
    return true;
}

bool ISOFairPlayStreamingKeyAssetIdBox::pack(MutableByteView& view, unsigned& offset) const
{
    if (!ISOBox::pack(view, offset))
        return false;

    return checkedWriteSequence<uint8_t>(m_data, view, offset, BigEndian);
}

bool ISOFairPlayStreamingKeyContextBox::parse(const ByteView& view, unsigned& offset)
{
    unsigned localOffset = offset;
    if (!ISOBox::parse(view, localOffset))
        return false;

    if (localOffset - offset == m_size) {
        m_data.clear();
        offset = localOffset;
        return true;
    }

    size_t dataSize;
    if (!WTF::safeSub(m_size, localOffset - offset, dataSize))
        return false;

    size_t remainingInView;
    if (!WTF::safeSub(view.size(), localOffset, remainingInView))
        return false;

    if (remainingInView < dataSize)
        return false;

    m_data.resize(dataSize);
    if (!checkedReadSequence<uint8_t>(m_data, view, localOffset, BigEndian))
        return false;

    offset = localOffset;
    return true;
}

bool ISOFairPlayStreamingKeyContextBox::pack(MutableByteView& view, unsigned& offset) const
{
    if (!ISOBox::pack(view, offset))
        return false;

    return checkedWriteSequence<uint8_t>(m_data, view, offset, BigEndian);
}

bool ISOFairPlayStreamingKeyVersionListBox::parse(const ByteView& view, unsigned& offset)
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

bool ISOFairPlayStreamingKeyVersionListBox::pack(MutableByteView& view, unsigned& offset) const
{
    if (!ISOBox::pack(view, offset))
        return false;

    return checkedWriteSequence<uint32_t>(m_versions, view, offset, BigEndian);
}

void ISOFairPlayStreamingKeyRequestBox::updateSize()
{
    m_requestInfo.updateSize();
    if (m_assetID)
        m_assetID->updateSize();
    if (m_context)
        m_context->updateSize();
    if (m_versionList)
        m_versionList->updateSize();
    ISOBox::updateSize();
}

uint64_t ISOFairPlayStreamingKeyRequestBox::partialSize() const
{
    auto size = ISOBox::partialSize();
    size += m_requestInfo.requiredSize();
    if (m_assetID)
        size += m_assetID->requiredSize();
    if (m_context)
        size += m_context->requiredSize();
    if (m_versionList)
        size += m_versionList->requiredSize();
    return size;
}

bool ISOFairPlayStreamingKeyRequestBox::parse(const ByteView& view, unsigned& offset)
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

            m_assetID = WTF::move(assetID);
            continue;
        }

        if (name == ISOFairPlayStreamingKeyContextBox::boxTypeName()) {
            if (m_context)
                return false;

            ISOFairPlayStreamingKeyContextBox context;
            if (!context.read(view, localOffset))
                return false;

            m_context = WTF::move(context);
            continue;
        }

        if (name == ISOFairPlayStreamingKeyVersionListBox::boxTypeName()) {
            if (m_versionList)
                return false;

            ISOFairPlayStreamingKeyVersionListBox versionList;
            if (!versionList.read(view, localOffset))
                return false;

            m_versionList = WTF::move(versionList);
            continue;
        }

        // Unknown box type; error.
        return false;
    }   
    
    offset = localOffset;
    return true;
}

bool ISOFairPlayStreamingKeyRequestBox::pack(MutableByteView& view, unsigned& offset) const
{
    if (!ISOBox::pack(view, offset))
        return false;

    if (!m_requestInfo.write(view, offset))
        return false;

    if (m_assetID && !m_assetID->write(view, offset))
        return false;

    if (m_context && !m_context->write(view, offset))
        return false;

    if (m_versionList && !m_versionList->write(view, offset))
        return false;

    return true;
}

bool ISOFairPlayStreamingInitDataBox::parse(const ByteView& view, unsigned& offset)
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

        m_requests.append(WTF::move(request));
    }

    offset = localOffset;
    return true;
}

bool ISOFairPlayStreamingPsshBox::parseData(const ByteView& view, unsigned& offset, uint64_t size)
{
    if (!m_initDataBox.read(view, offset))
        return false;
    ASSERT_UNUSED(size, m_initDataBox.size() == size);
    return true;
}

bool ISOFairPlayStreamingInitDataBox::pack(MutableByteView& view, unsigned& offset) const
{
    if (!ISOBox::pack(view, offset))
        return false;

    if (!m_info.write(view, offset))
        return false;

    for (auto& request : m_requests) {
        if (!request.write(view, offset))
            return false;
    }

    return true;
}

void ISOFairPlayStreamingInitDataBox::updateSize()
{
    m_info.updateSize();
    for (auto& request : m_requests)
        request.updateSize();
    ISOBox::updateSize();
}

uint64_t ISOFairPlayStreamingInitDataBox::partialSize() const
{
    auto size = ISOBox::partialSize();
    size += m_info.requiredSize();
    for (auto& request : m_requests)
        size += request.requiredSize();
    return size;
}

uint64_t ISOFairPlayStreamingPsshBox::dataSize() const
{
    return m_initDataBox.size();
}

bool ISOFairPlayStreamingPsshBox::writeData(MutableByteView& view, unsigned& offset) const
{
    return m_initDataBox.write(view, offset);
}

void ISOFairPlayStreamingPsshBox::updateSize()
{
    m_initDataBox.updateSize();
    ISOBox::updateSize();
}

uint64_t ISOFairPlayStreamingPsshBox::partialSize() const
{
    // ISOProtectionSystemSpecificHeaderBox::partialSize() adds its base size
    // to the size of m_data. However, as we've overridden parseData(), it should
    // be the case that m_data is empty. Assert so, and just in case, subtract out
    // its size from our super classes' partial size.
    ASSERT(!m_data.size());
    return ISOProtectionSystemSpecificHeaderBox::partialSize()
        - m_data.size()
        + m_initDataBox.requiredSize();
}

ISOFairPlayStreamingInfoBox::ISOFairPlayStreamingInfoBox()
    : ISOFullBox(boxTypeName(), 0, 0)
{
}

ISOFairPlayStreamingInfoBox::ISOFairPlayStreamingInfoBox(const ISOFairPlayStreamingInfoBox&) = default;
ISOFairPlayStreamingInfoBox::ISOFairPlayStreamingInfoBox(ISOFairPlayStreamingInfoBox&&) = default;
ISOFairPlayStreamingInfoBox::~ISOFairPlayStreamingInfoBox() = default;

ISOFairPlayStreamingKeyRequestInfoBox::ISOFairPlayStreamingKeyRequestInfoBox()
    : ISOFullBox(boxTypeName(), 0, 0)
{
    // The key ID is a fixed-width field, so a default-constructed box has to start out with one:
    // parse() requires capacity() bytes to be present, and a box written without them would not
    // be readable back. Match the length parse() expects rather than hard-coding it here.
    m_keyID.fill(0, m_keyID.capacity());
}

ISOFairPlayStreamingKeyRequestInfoBox::ISOFairPlayStreamingKeyRequestInfoBox(const ISOFairPlayStreamingKeyRequestInfoBox&) = default;
ISOFairPlayStreamingKeyRequestInfoBox::ISOFairPlayStreamingKeyRequestInfoBox(ISOFairPlayStreamingKeyRequestInfoBox&&) = default;
ISOFairPlayStreamingKeyRequestInfoBox::~ISOFairPlayStreamingKeyRequestInfoBox() = default;

ISOFairPlayStreamingKeyAssetIdBox::ISOFairPlayStreamingKeyAssetIdBox()
    : ISOBox(boxTypeName())
{
}

ISOFairPlayStreamingKeyAssetIdBox::ISOFairPlayStreamingKeyAssetIdBox(const ISOFairPlayStreamingKeyAssetIdBox&) = default;
ISOFairPlayStreamingKeyAssetIdBox::ISOFairPlayStreamingKeyAssetIdBox(ISOFairPlayStreamingKeyAssetIdBox&&) = default;
ISOFairPlayStreamingKeyAssetIdBox::~ISOFairPlayStreamingKeyAssetIdBox() = default;

ISOFairPlayStreamingKeyContextBox::ISOFairPlayStreamingKeyContextBox()
    : ISOBox(boxTypeName())
{
}

ISOFairPlayStreamingKeyContextBox::ISOFairPlayStreamingKeyContextBox(const ISOFairPlayStreamingKeyContextBox&) = default;
ISOFairPlayStreamingKeyContextBox::ISOFairPlayStreamingKeyContextBox(ISOFairPlayStreamingKeyContextBox&&) = default;
ISOFairPlayStreamingKeyContextBox::~ISOFairPlayStreamingKeyContextBox() = default;

ISOFairPlayStreamingKeyVersionListBox::ISOFairPlayStreamingKeyVersionListBox()
    : ISOBox(boxTypeName())
{
}

ISOFairPlayStreamingKeyVersionListBox::ISOFairPlayStreamingKeyVersionListBox(const ISOFairPlayStreamingKeyVersionListBox&) = default;
ISOFairPlayStreamingKeyVersionListBox::ISOFairPlayStreamingKeyVersionListBox(ISOFairPlayStreamingKeyVersionListBox&&) = default;
ISOFairPlayStreamingKeyVersionListBox::~ISOFairPlayStreamingKeyVersionListBox() = default;

ISOFairPlayStreamingKeyRequestBox::ISOFairPlayStreamingKeyRequestBox()
    : ISOBox(boxTypeName())
{
}

ISOFairPlayStreamingKeyRequestBox::ISOFairPlayStreamingKeyRequestBox(const ISOFairPlayStreamingKeyRequestBox&) = default;
ISOFairPlayStreamingKeyRequestBox::ISOFairPlayStreamingKeyRequestBox(ISOFairPlayStreamingKeyRequestBox&&) = default;
ISOFairPlayStreamingKeyRequestBox::~ISOFairPlayStreamingKeyRequestBox() = default;

ISOFairPlayStreamingInitDataBox::ISOFairPlayStreamingInitDataBox()
    : ISOBox(boxTypeName())
{
}

ISOFairPlayStreamingInitDataBox::~ISOFairPlayStreamingInitDataBox() = default;

ISOFairPlayStreamingPsshBox::ISOFairPlayStreamingPsshBox()
    : ISOProtectionSystemSpecificHeaderBox(fairPlaySystemID())
{
}

ISOFairPlayStreamingPsshBox::~ISOFairPlayStreamingPsshBox() = default;

} // namespace WebCore
