/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>. All rights reserved.
 * Copyright (C) 2017 Mozilla Foundation. All rights reserved.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "config.h"
#include <wtf/text/StringBuilderJSON.h>

#include <wtf/text/EscapedFormsForJSON.h>
#include <wtf/text/WTFString.h>

namespace WTF {

void StringBuilder::appendQuotedJSONString(const String& string)
{
    if (hasOverflowed())
        return;

    // Make sure we have enough buffer space to append this string for worst case without reallocating.
    // The 2 is for the '"' quotes on each end.
    // The 6 is the worst case for a single code unit that could be encoded as \uNNNN.
    CheckedUint32 stringLength = string.length();
    stringLength *= 6;
    stringLength += 2;
    if (stringLength.hasOverflowed()) {
        didOverflow();
        return;
    }

    auto stringLengthValue = stringLength.value();

    if (is8Bit() && string.is8Bit()) {
        if (auto* output = extendBufferForAppending<LChar>(saturatedSum<uint32_t>(m_length, stringLengthValue))) {
            auto* end = output + stringLengthValue;
            *output++ = '"';
            appendEscapedJSONStringContent(output, string.span8());
            *output++ = '"';
            if (output < end)
                shrink(m_length - (end - output));
        }
    } else {
        if (auto* output = extendBufferForAppendingWithUpconvert(saturatedSum<uint32_t>(m_length, stringLengthValue))) {
            auto* end = output + stringLengthValue;
            *output++ = '"';
            if (string.is8Bit())
                appendEscapedJSONStringContent(output, string.span8());
            else
                appendEscapedJSONStringContent(output, string.span16());
            *output++ = '"';
            if (output < end)
                shrink(m_length - (end - output));
        }
    }
}

} // namespace WTF
