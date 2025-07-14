/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "ArrayAllocationProfile.h"
#include "ConcurrentJSLock.h"
#include "ValueProfile.h"
#include <variant>
#include <wtf/TZoneMalloc.h>

namespace JSC::BuiltinProfiling {

class BuiltinProfileBase {
public:
    virtual ~BuiltinProfileBase() = default;

    mutable ConcurrentJSLock m_lock;

    virtual void updateValueProfiles(const ConcurrentJSLocker&) = 0;
};

class ArrayPrototypeFilterProfile : public BuiltinProfileBase {
public:
    ArrayPrototypeFilterProfile() = default;

public:
    ValueProfile m_thisValueProfile;
    ValueProfile m_toObjectValueProfile;
    ValueProfile m_toLengthValueProfile;
    ValueProfile m_getByValValueProfile;
    ValueProfile m_callbackResValueProfile;

    ArrayAllocationProfile m_arrayAllocProfile;
    // ValueProfile m_thisArgValueProfile;
    // ValueProfile m_callbackReturnValueProfile;

    void updateValueProfiles(const ConcurrentJSLocker& locker) override
    {
        m_thisValueProfile.computeUpdatedPrediction(locker);
        m_toObjectValueProfile.computeUpdatedPrediction(locker);
        m_toLengthValueProfile.computeUpdatedPrediction(locker);
        m_getByValValueProfile.computeUpdatedPrediction(locker);
        m_callbackResValueProfile.computeUpdatedPrediction(locker);
    }
};

#define JSC_BUILTIN_PROFILE_TYPE_FOREACH(macro) \
    macro(ArrayPrototypeFilterProfile)

using BuiltinProfileInvalid = struct { };
#define X(name) , name
using BuiltinProfileVariant = std::variant<
    BuiltinProfileInvalid
    JSC_BUILTIN_PROFILE_TYPE_FOREACH(X)
>;
#undef X

#define X(name) || std::is_same_v<T, name>
template <typename T>
concept IsBuiltinProfileType = false JSC_BUILTIN_PROFILE_TYPE_FOREACH(X);
#undef X

class BuiltinProfile {
WTF_MAKE_TZONE_ALLOCATED(BuiltinProfile);
private:
    BuiltinProfileVariant m_variant;

public:
    template <IsBuiltinProfileType T, typename... Args>
    inline explicit BuiltinProfile(std::in_place_type_t<T>, Args&&... args) : m_variant(std::in_place_type<T>, std::forward<Args>(args)...) { }

    template <IsBuiltinProfileType T>
    inline explicit BuiltinProfile(std::in_place_type_t<T>) : m_variant(std::in_place_type<T>) { }

    inline BuiltinProfileVariant& variant() { return m_variant; }
    inline const BuiltinProfileVariant& variant() const { return m_variant; }
};



} // namespace JSC::BuiltinProfiling

