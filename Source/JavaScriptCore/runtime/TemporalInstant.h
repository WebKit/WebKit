/*
 * Copyright (C) 2021 Igalia S.L.
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "ISO8601.h"
#include "JSObject.h"
#include "TemporalDuration.h"
#include "TemporalObject.h"
#include "VM.h"
#include <wtf/Packed.h>

namespace JSC {

class TemporalInstant final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.temporalInstantSpace<mode>();
    }

    static TemporalInstant* create(VM&, Structure*, ISO8601::ExactTime);
    static TemporalInstant* tryCreateIfValid(JSGlobalObject*, ISO8601::ExactTime, Structure* = nullptr);
    static TemporalInstant* tryCreateIfValid(JSGlobalObject*, JSValue, Structure* = nullptr);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    static TemporalInstant* toInstant(JSGlobalObject*, JSValue);
    static TemporalInstant* from(JSGlobalObject*, JSValue);
    static TemporalInstant* fromEpochMilliseconds(JSGlobalObject*, JSValue);
    static TemporalInstant* fromEpochNanoseconds(JSGlobalObject*, JSValue);
    static JSValue compare(JSGlobalObject*, JSValue, JSValue);

    ISO8601::ExactTime exactTime() const { return m_exactTime.get(); }

    ISO8601::Duration difference(JSGlobalObject*, TemporalInstant*, JSValue options) const;
    ISO8601::ExactTime round(JSGlobalObject*, JSValue options) const;
    String toString(JSGlobalObject*, JSValue options) const;
    String toString(JSObject* timeZone = nullptr, PrecisionData precision = { { Precision::Auto, 0 }, TemporalUnit::Nanosecond, 1 }) const
    {
        return toString(exactTime(), timeZone, precision);
    }

private:
    TemporalInstant(VM&, Structure*, ISO8601::ExactTime);

    template<typename CharacterType>
    static std::optional<ISO8601::ExactTime> parse(StringParsingBuffer<CharacterType>&);
    static ISO8601::ExactTime fromObject(JSGlobalObject*, JSObject*);

    static String toString(ISO8601::ExactTime, JSObject* timeZone, PrecisionData);

    Packed<ISO8601::ExactTime> m_exactTime;
};

} // namespace JSC
