/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "ISOTrackEncryptionBox.h"

#include <JavaScriptCore/DataView.h>

using JSC::DataView;

namespace WebCore {

bool ISOTrackEncryptionBox::parse(DataView& view, unsigned& offset)
{
    // ISO/IEC 23001-7-2015 Section 8.2.2
    if (!ISOFullBox::parse(view, offset))
        return false;

    // unsigned int(8) reserved = 0;
    offset += 1;

    if (!m_version) {
        // unsigned int(8) reserved = 0;
        offset += 1;
    } else {
        int8_t cryptAndSkip = 0;
        if (!checkedRead<int8_t>(cryptAndSkip, view, offset, BigEndian))
            return false;

        m_defaultCryptByteBlock = cryptAndSkip >> 4;
        m_defaultSkipByteBlock = cryptAndSkip & 0xF;
    }

    if (!checkedRead<int8_t>(m_defaultIsProtected, view, offset, BigEndian))
        return false;

    if (!checkedRead<int8_t>(m_defaultPerSampleIVSize, view, offset, BigEndian))
        return false;

    auto buffer = view.possiblySharedBuffer();
    if (!buffer)
        return false;

    auto keyIDBuffer = buffer->slice(offset, offset + 16);
    if (!keyIDBuffer)
        return false;

    offset += 16;

    m_defaultKID.resize(16);
    memcpy(m_defaultKID.data(), keyIDBuffer->data(), 16);

    if (m_defaultIsProtected == 1 && !m_defaultPerSampleIVSize) {
        int8_t defaultConstantIVSize = 0;
        if (!checkedRead<int8_t>(defaultConstantIVSize, view, offset, BigEndian))
            return false;

        Vector<uint8_t> defaultConstantIV;
        defaultConstantIV.reserveInitialCapacity(defaultConstantIVSize);
        while (defaultConstantIVSize--) {
            int8_t character = 0;
            if (!checkedRead<int8_t>(character, view, offset, BigEndian))
                return false;
            defaultConstantIV.uncheckedAppend(character);
        }
        m_defaultConstantIV = WTFMove(defaultConstantIV);
    }

    return true;
}

}
