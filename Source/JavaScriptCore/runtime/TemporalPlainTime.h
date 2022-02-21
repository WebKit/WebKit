/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
#include "LazyProperty.h"
#include "TemporalCalendar.h"

namespace JSC {

class TemporalPlainTime final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.temporalPlainTimeSpace<mode>();
    }

    static TemporalPlainTime* create(VM&, Structure*, ISO8601::PlainTime&&);
    static TemporalPlainTime* tryCreateIfValid(JSGlobalObject*, Structure*, ISO8601::Duration&&);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    static TemporalPlainTime* from(JSGlobalObject*, JSValue, std::optional<TemporalOverflow>);
    static int32_t compare(TemporalPlainTime*, TemporalPlainTime*);

    TemporalCalendar* calendar() { return m_calendar.get(this); }
    ISO8601::PlainTime plainTime() const { return m_plainTime; }

#define JSC_DEFINE_TEMPORAL_PLAIN_TIME_FIELD(name, capitalizedName) \
    unsigned name() const { return m_plainTime.name(); }
    JSC_TEMPORAL_PLAIN_TIME_UNITS(JSC_DEFINE_TEMPORAL_PLAIN_TIME_FIELD);
#undef JSC_DEFINE_TEMPORAL_PLAIN_TIME_FIELD

    ISO8601::PlainTime with(JSGlobalObject*, JSObject* temporalTimeLike, JSValue options) const;
    ISO8601::PlainTime add(JSGlobalObject*, JSValue) const;
    ISO8601::PlainTime subtract(JSGlobalObject*, JSValue) const;
    ISO8601::PlainTime round(JSGlobalObject*, JSValue options) const;
    String toString(JSGlobalObject*, JSValue options) const;
    String toString(std::tuple<Precision, unsigned> precision = { Precision::Auto, 0 }) const
    {
        return ISO8601::temporalTimeToString(m_plainTime, precision);
    }

    ISO8601::Duration until(JSGlobalObject*, TemporalPlainTime*, JSValue options) const;
    ISO8601::Duration since(JSGlobalObject*, TemporalPlainTime*, JSValue options) const;

    DECLARE_VISIT_CHILDREN;

private:
    TemporalPlainTime(VM&, Structure*, ISO8601::PlainTime&&);
    void finishCreation(VM&);

    template<typename CharacterType>
    static std::optional<ISO8601::PlainTime> parse(StringParsingBuffer<CharacterType>&);
    static ISO8601::PlainTime fromObject(JSGlobalObject*, JSObject*);

    ISO8601::PlainTime m_plainTime;
    LazyProperty<TemporalPlainTime, TemporalCalendar> m_calendar;
};

} // namespace JSC
