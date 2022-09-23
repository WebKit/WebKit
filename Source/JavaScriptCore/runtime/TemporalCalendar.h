/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2022 Sony Interactive Entertainment Inc.
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
#include "IntlObject.h"
#include "JSObject.h"

namespace JSC {

class TemporalCalendar final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.temporalCalendarSpace<mode>();
    }

    static TemporalCalendar* create(VM&, Structure*, CalendarID);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    static JSObject* toTemporalCalendarWithISODefault(JSGlobalObject*, JSValue);
    static JSObject* getTemporalCalendarWithISODefault(JSGlobalObject*, JSValue);
    static ISO8601::PlainDate isoDateFromFields(JSGlobalObject*, JSObject*, TemporalOverflow);
    static ISO8601::PlainDate isoDateFromFields(JSGlobalObject*, double year, double month, double day, TemporalOverflow);
    static ISO8601::PlainDate isoDateAdd(JSGlobalObject*, const ISO8601::PlainDate&, const ISO8601::Duration&, TemporalOverflow);
    static ISO8601::Duration isoDateDifference(JSGlobalObject*, const ISO8601::PlainDate&, const ISO8601::PlainDate&, TemporalUnit);
    static int32_t isoDateCompare(const ISO8601::PlainDate&, const ISO8601::PlainDate&);

    CalendarID identifier() const { return m_identifier; }
    bool isISO8601() const { return m_identifier == iso8601CalendarID(); }

    static std::optional<CalendarID> isBuiltinCalendar(StringView);

    static JSObject* from(JSGlobalObject*, JSValue);

    bool equals(JSGlobalObject*, TemporalCalendar*);

private:
    TemporalCalendar(VM&, Structure*, CalendarID);

    CalendarID m_identifier { 0 };
};

} // namespace JSC
