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
#include "ISOSchemeInformationBox.h"

#include "ISOTrackEncryptionBox.h"
#include <JavaScriptCore/DataView.h>

using JSC::DataView;

namespace WebCore {

bool ISOSchemeInformationBox::parse(DataView& view, unsigned& offset)
{
    unsigned localOffset = offset;
    if (!ISOBox::parse(view, localOffset))
        return false;

    auto schemeSpecificBoxType = ISOBox::peekBox(view, localOffset);
    if (!schemeSpecificBoxType)
        return false;

    if (schemeSpecificBoxType.value().first == ISOTrackEncryptionBox::boxTypeName()) {
        if (localOffset + schemeSpecificBoxType.value().second > offset + m_size)
            return false;

        m_schemeSpecificData = makeUnique<ISOTrackEncryptionBox>();
        if (!m_schemeSpecificData->read(view, localOffset))
            return false;
    }

    return true;
}

}
