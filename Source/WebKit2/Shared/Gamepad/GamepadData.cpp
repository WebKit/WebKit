/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "GamepadData.h"

#if ENABLE(GAMEPAD)

#include "ArgumentCoders.h"
#include <wtf/text/StringBuilder.h>

namespace WebKit {

void GamepadData::encode(IPC::Encoder& encoder) const
{
    encoder << index << axisValues << buttonValues;
}

bool GamepadData::decode(IPC::Decoder& decoder, GamepadData& data)
{
    if (!decoder.decode(data.index))
        return false;

    if (!decoder.decode(data.axisValues))
        return false;

    if (!decoder.decode(data.buttonValues))
        return false;

    return true;
}

bool GamepadData::isNull() const
{
    return !index && axisValues.isEmpty() && buttonValues.isEmpty();
}

#if !LOG_DISABLED
String GamepadData::loggingString() const
{
    StringBuilder builder;

    builder.appendNumber(axisValues.size());
    builder.appendLiteral(" axes, ");
    builder.appendNumber(buttonValues.size());
    builder.appendLiteral(" buttons\n");

    for (size_t i = 0; i < axisValues.size(); ++i) {
        builder.appendLiteral(" Axis ");
        builder.appendNumber(i);
        builder.appendLiteral(": ");
        builder.appendNumber(axisValues[i]);
    }

    builder.append('\n');
    for (size_t i = 0; i < buttonValues.size(); ++i) {
        builder.appendLiteral(" Button ");
        builder.appendNumber(i);
        builder.appendLiteral(": ");
        builder.appendNumber(buttonValues[i]);
    }

    return builder.toString();
}
#endif

} // namespace WebKit

#endif // ENABLE(GAMEPAD)
