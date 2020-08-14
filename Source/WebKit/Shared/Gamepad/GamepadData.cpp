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

using WebCore::SharedGamepadValue;

namespace WebKit {

GamepadData::GamepadData(unsigned index, const String& id, const String& mapping, const Vector<SharedGamepadValue>& axisValues, const Vector<SharedGamepadValue>& buttonValues, MonotonicTime lastUpdateTime)
    : m_index(index)
    , m_id(id)
    , m_mapping(mapping)
    , m_axisValues(WTF::map(axisValues, [](const auto& value) { return value.value(); }))
    , m_buttonValues(WTF::map(buttonValues, [](const auto& value) { return value.value(); }))
    , m_lastUpdateTime(lastUpdateTime)
{
}

void GamepadData::encode(IPC::Encoder& encoder) const
{
    encoder << m_isNull;
    if (m_isNull)
        return;

    encoder << m_index << m_id << m_mapping << m_axisValues << m_buttonValues << m_lastUpdateTime;
}

Optional<GamepadData> GamepadData::decode(IPC::Decoder& decoder)
{
    GamepadData data;
    if (!decoder.decode(data.m_isNull))
        return WTF::nullopt;

    if (data.m_isNull)
        return data;

    if (!decoder.decode(data.m_index))
        return WTF::nullopt;

    if (!decoder.decode(data.m_id))
        return WTF::nullopt;

    if (!decoder.decode(data.m_mapping))
        return WTF::nullopt;

    if (!decoder.decode(data.m_axisValues))
        return WTF::nullopt;

    if (!decoder.decode(data.m_buttonValues))
        return WTF::nullopt;

    if (!decoder.decode(data.m_lastUpdateTime))
        return WTF::nullopt;

    return WTFMove(data);
}

#if !LOG_DISABLED

String GamepadData::loggingString() const
{
    StringBuilder builder;

    builder.append(m_axisValues.size(), " axes, ", m_buttonValues.size(), " buttons\n");

    for (size_t i = 0; i < m_axisValues.size(); ++i)
        builder.append(" Axis ", i, ": ", m_axisValues[i]);

    builder.append('\n');

    for (size_t i = 0; i < m_buttonValues.size(); ++i)
        builder.append(" Button ", i, ": ", FormattedNumber::fixedPrecision(m_buttonValues[i]));

    return builder.toString();
}

#endif

} // namespace WebKit

#endif // ENABLE(GAMEPAD)
