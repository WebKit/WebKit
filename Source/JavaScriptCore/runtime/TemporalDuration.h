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

#include "ISO8601.h"

namespace JSC {

class TemporalDuration final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;
    using Subdurations = ISO8601::Duration;

    template<typename CellType, SubspaceAccess mode>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.temporalDurationSpace<mode>();
    }

    static TemporalDuration* create(VM&, Structure*, Subdurations&&);
    static TemporalDuration* tryCreateIfValid(JSGlobalObject*, Subdurations&&, Structure* = nullptr);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    static TemporalDuration* toDuration(JSGlobalObject*, JSValue);
    static TemporalDuration* from(JSGlobalObject*, JSValue);
    static JSValue compare(JSGlobalObject*, JSValue, JSValue);

#define JSC_DEFINE_TEMPORAL_DURATION_FIELD(name, capitalizedName) \
    double name##s() const { return m_subdurations.name##s(); } \
    void set##capitalizedName##s(double value) { m_subdurations.set##capitalizedName##s(value); }
    JSC_TEMPORAL_UNITS(JSC_DEFINE_TEMPORAL_DURATION_FIELD);
#undef JSC_DEFINE_TEMPORAL_DURATION_FIELD

    int sign() const { return sign(m_subdurations); }

    Subdurations with(JSGlobalObject*, JSObject* durationLike) const;
    Subdurations negated() const;
    Subdurations abs() const;
    Subdurations add(JSGlobalObject*, JSValue) const;
    Subdurations subtract(JSGlobalObject*, JSValue) const;
    Subdurations round(JSGlobalObject*, JSValue options) const;
    double total(JSGlobalObject*, JSValue options) const;
    String toString(JSGlobalObject*, JSValue options) const;
    String toString(std::optional<unsigned> precision = std::nullopt) const { return toString(m_subdurations, precision); }

private:
    TemporalDuration(VM&, Structure*, Subdurations&&);
    void finishCreation(VM&);

    template<typename CharacterType>
    static std::optional<Subdurations> parse(StringParsingBuffer<CharacterType>&);
    static Subdurations fromObject(JSGlobalObject*, JSObject*);

    static int sign(const Subdurations&);
    static void balance(Subdurations&, TemporalUnit largestUnit);
    static double round(Subdurations&, double increment, TemporalUnit, RoundingMode);
    static String toString(const Subdurations&, std::optional<unsigned> precision);

    TemporalUnit largestSubduration() const;

    Subdurations m_subdurations;
};

} // namespace JSC
