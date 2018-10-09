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
#include "ISOProtectionSchemeInfoBox.h"

#include "ISOSchemeInformationBox.h"
#include "ISOSchemeTypeBox.h"
#include <JavaScriptCore/DataView.h>

using JSC::DataView;

namespace WebCore {

bool ISOProtectionSchemeInfoBox::parse(DataView& view, unsigned& offset)
{
    unsigned localOffset = offset;
    if (!ISOBox::parse(view, localOffset))
        return false;

    if (!m_originalFormatBox.read(view, localOffset))
        return false;

    if (localOffset - offset == m_size) {
        offset = localOffset;
        return true;
    }

    auto optionalBoxType = ISOBox::peekBox(view, localOffset);
    if (!optionalBoxType)
        return false;

    if (optionalBoxType.value().first == ISOSchemeTypeBox::boxTypeName()) {
        m_schemeTypeBox = std::make_unique<ISOSchemeTypeBox>();
        if (!m_schemeTypeBox->read(view, localOffset))
            return false;

        if (localOffset - offset == m_size) {
            offset = localOffset;
            return true;
        }

        optionalBoxType = ISOBox::peekBox(view, localOffset);
        if (!optionalBoxType)
            return false;
    }

    if (optionalBoxType.value().first == ISOSchemeInformationBox::boxTypeName()) {
        m_schemeInformationBox = std::make_unique<ISOSchemeInformationBox>();
        if (!m_schemeInformationBox->read(view, localOffset))
            return false;

        if (localOffset - offset != m_size)
            return false;
    }

    offset = localOffset;
    return true;
}

}
