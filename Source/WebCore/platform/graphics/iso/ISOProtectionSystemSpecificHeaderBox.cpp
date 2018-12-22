/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018 Igalia S.L
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
#include "ISOProtectionSystemSpecificHeaderBox.h"

#include <JavaScriptCore/DataView.h>

using JSC::DataView;

namespace WebCore {

bool ISOProtectionSystemSpecificHeaderBox::parse(DataView& view, unsigned& offset)
{
    if (!ISOFullBox::parse(view, offset))
        return false;

    // ISO/IEC 23001-7-2016 Section 8.1.1
    auto buffer = view.possiblySharedBuffer();
    if (!buffer)
        return false;
    auto systemID = buffer->slice(offset, offset + 16);
    offset += 16;

    m_systemID.resize(16);
    memcpy(m_systemID.data(), systemID->data(), 16);

    if (m_version) {
        uint32_t keyIDCount = 0;
        if (!checkedRead<uint32_t>(keyIDCount, view, offset, BigEndian))
            return false;
        if (buffer->byteLength() - offset < keyIDCount * 16)
            return false;
        m_keyIDs.resize(keyIDCount);
        for (unsigned keyID = 0; keyID < keyIDCount; keyID++) {
            auto& currentKeyID = m_keyIDs[keyID];
            currentKeyID.resize(16);
            auto parsedKeyID = buffer->slice(offset, offset + 16);
            offset += 16;
            memcpy(currentKeyID.data(), parsedKeyID->data(), 16);
        }
    }

    uint32_t dataSize = 0;
    if (!checkedRead<uint32_t>(dataSize, view, offset, BigEndian))
        return false;
    if (buffer->byteLength() - offset < dataSize)
        return false;
    auto parsedData = buffer->slice(offset, offset + dataSize);
    offset += dataSize;

    m_data.resize(dataSize);
    memcpy(m_data.data(), parsedData->data(), dataSize);

    return true;
}

}
