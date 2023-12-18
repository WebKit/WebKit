/*
 * Copyright (C) 2015-2023 Apple Inc. All rights reserved.
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

#include "ObjectPropertyCondition.h"
#include "Watchpoint.h"
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>

namespace JSC {

// FIXME: This isn't actually a Watchpoint. We should probably have a name which better reflects that:
// https://bugs.webkit.org/show_bug.cgi?id=202381
class AdaptiveInferredPropertyValueWatchpointBase {
    WTF_MAKE_NONCOPYABLE(AdaptiveInferredPropertyValueWatchpointBase);
    WTF_MAKE_TZONE_ALLOCATED(AdaptiveInferredPropertyValueWatchpointBase);

public:
    AdaptiveInferredPropertyValueWatchpointBase(const ObjectPropertyCondition&);
    AdaptiveInferredPropertyValueWatchpointBase() = default;

    const ObjectPropertyCondition& key() const { return m_key; }

    void initialize(const ObjectPropertyCondition&);
    void install(VM&);

    virtual ~AdaptiveInferredPropertyValueWatchpointBase() = default;

    class StructureWatchpoint final : public Watchpoint {
    public:
        StructureWatchpoint()
            : Watchpoint(Watchpoint::Type::AdaptiveInferredPropertyValueStructure)
        { }

        void fireInternal(VM&, const FireDetail&);
    };
    static_assert(sizeof(StructureWatchpoint) == sizeof(Watchpoint));

    class PropertyWatchpoint final : public Watchpoint {
    public:
        PropertyWatchpoint()
            : Watchpoint(Watchpoint::Type::AdaptiveInferredPropertyValueProperty)
        { }

        void fireInternal(VM&, const FireDetail&);
    };
    static_assert(sizeof(PropertyWatchpoint) == sizeof(Watchpoint));

protected:
    virtual bool isValid() const;
    virtual void handleFire(VM&, const FireDetail&) = 0;

private:
    void fire(VM&, const FireDetail&);

    ObjectPropertyCondition m_key;
    StructureWatchpoint m_structureWatchpoint;
    PropertyWatchpoint m_propertyWatchpoint;
};

} // namespace JSC
