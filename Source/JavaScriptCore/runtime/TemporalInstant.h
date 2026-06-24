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

#include <JavaScriptCore/ISO8601.h>
#include <JavaScriptCore/InstantCore.h>
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/TemporalDuration.h>
#include <JavaScriptCore/TemporalObject.h>
#include <JavaScriptCore/VM.h>

namespace JSC {

class TemporalInstant final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;

    static constexpr uint8_t numberOfLowerTierPreciseCells = 0;

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.temporalInstantSpace<mode>();
    }

    static TemporalInstant* create(VM&, Structure*, ISO8601::ExactTime);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    static TemporalInstant* toInstant(JSGlobalObject*, JSValue);
    static TemporalInstant* fromEpochMilliseconds(JSGlobalObject*, JSValue);
    static TemporalInstant* fromEpochNanoseconds(JSGlobalObject*, JSValue);
    static JSValue compare(JSGlobalObject*, JSValue, JSValue);

    ISO8601::ExactTime exactTime() const { return m_exactTime; }

    String toString(PrecisionData precision = { { Precision::Auto, 0 }, TemporalUnit::Nanosecond, 1 }) const
    {
        return TemporalCore::instantToString(exactTime(), std::nullopt, precision);
    }

private:
    TemporalInstant(VM&, Structure*, ISO8601::ExactTime);

    ISO8601::ExactTime m_exactTime;
};

JS_EXPORT_PRIVATE std::optional<ISO8601::ExactTime> bigIntValueToExactTime(JSGlobalObject*, JSValue bigIntValue, ASCIILiteral typeName);

} // namespace JSC
