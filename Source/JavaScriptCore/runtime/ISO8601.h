/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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

#pragma once

#include "TemporalObject.h"

namespace JSC {
namespace ISO8601 {

struct Duration {
    using const_iterator = std::array<double, numberOfTemporalUnits>::const_iterator;

#define JSC_DEFINE_ISO8601_DURATION_FIELD(name, capitalizedName) \
    double name##s() const { return data[static_cast<uint8_t>(TemporalUnit::capitalizedName)]; } \
    void set##capitalizedName##s(double value) { data[static_cast<uint8_t>(TemporalUnit::capitalizedName)] = value; }
    JSC_TEMPORAL_UNITS(JSC_DEFINE_ISO8601_DURATION_FIELD);
#undef JSC_DEFINE_ISO8601_DURATION_FIELD

    double& operator[](size_t i) { return data[i]; }
    const double& operator[](size_t i) const { return data[i]; }
    const_iterator begin() const { return data.begin(); }
    const_iterator end() const { return data.end(); }
    void clear() { data.fill(0); }

    std::array<double, numberOfTemporalUnits> data { };
};

std::optional<Duration> parseDuration(StringView);

} // namespace ISO8601
} // namespace JSC
