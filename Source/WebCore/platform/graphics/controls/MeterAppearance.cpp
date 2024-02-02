/*
 * Copyright (C) 2022 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MeterAppearance.h"

#include "ControlFactory.h"
#include "PlatformControl.h"

namespace WebCore {

MeterAppearance::MeterAppearance()
    : m_gaugeRegion(GaugeRegion::EvenLessGood)
    , m_value(0)
    , m_minimum(0)
    , m_maximum(0)
{
}

MeterAppearance::MeterAppearance(GaugeRegion gaugeRegion, double value, double minimum, double maximum)
    : m_gaugeRegion(gaugeRegion)
    , m_value(value)
    , m_minimum(minimum)
    , m_maximum(maximum)
{
}

std::unique_ptr<PlatformControl> MeterAppearance::createPlatformControl(ControlPart& part, ControlFactory& controlFactory)
{
    return controlFactory.createPlatformMeter(part);
}

} // namespace WebCore
