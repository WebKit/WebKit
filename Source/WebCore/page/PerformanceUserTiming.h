/*
 * Copyright (C) 2012 Intel Inc. All rights reserved.
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

#pragma once

#include "PerformanceMark.h"
#include "PerformanceMeasure.h"
#include "PerformanceMeasureOptions.h"
#include <wtf/HashMap.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakRef.h>
#include <wtf/text/StringHash.h>

namespace JSC {
class JSGlobalObject;
}

namespace WebCore {

class Performance;
class WeakPtrImplWithEventTargetData;
template<typename> class ExceptionOr;

using PerformanceEntryMap = HashMap<RefPtr<StringImpl>, Vector<Ref<PerformanceEntry>>>;

class PerformanceUserTiming {
    WTF_MAKE_TZONE_ALLOCATED(PerformanceUserTiming);
public:
    explicit PerformanceUserTiming(Performance&);

    ExceptionOr<Ref<PerformanceMark>> mark(JSC::JSGlobalObject&, StringImpl* markName, std::optional<PerformanceMarkOptions>&&);
    void clearMarks(StringImpl* markName);

    using StartOrMeasureOptions = Variant<RefPtr<StringImpl>, PerformanceMeasureOptions>;
    ExceptionOr<Ref<PerformanceMeasure>> measure(JSC::JSGlobalObject&, StringImpl* measureName, StartOrMeasureOptions&&, StringImpl* endMark);
    void clearMeasures(StringImpl* measureName);

    Vector<Ref<PerformanceEntry>> getMarks() const;
    Vector<Ref<PerformanceEntry>> getMeasures() const;

    Vector<Ref<PerformanceEntry>> getMarks(StringImpl* name) const;
    Vector<Ref<PerformanceEntry>> getMeasures(StringImpl* name) const;

    static bool isRestrictedMarkName(StringImpl* markName);

private:
    ExceptionOr<double> convertMarkToTimestamp(const Variant<RefPtr<StringImpl>, double>&) const;
    ExceptionOr<double> convertMarkToTimestamp(StringImpl* markName) const;
    ExceptionOr<double> convertMarkToTimestamp(double) const;

    ExceptionOr<Ref<PerformanceMeasure>> measure(StringImpl* measureName, StringImpl* startMark, StringImpl* endMark);
    ExceptionOr<Ref<PerformanceMeasure>> measure(JSC::JSGlobalObject&, StringImpl* measureName, const PerformanceMeasureOptions&);

    WeakRef<Performance, WeakPtrImplWithEventTargetData> m_performance;
    PerformanceEntryMap m_marksMap;
    PerformanceEntryMap m_measuresMap;
};

}
