/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "InspectorFrontendChannel.h"
#include <wtf/text/WTFString.h>

namespace Inspector {

// FIXME: Add DedicatedWorker Inspector Targets
// FIXME: Add ServiceWorker Inspector Targets
enum class InspectorTargetType : uint8_t {
    JavaScriptContext,
    Page,
    DedicatedWorker,
    ServiceWorker,
};

class JS_EXPORT_PRIVATE InspectorTarget {
public:
    virtual ~InspectorTarget() = default;

    // State.
    virtual String identifier() const = 0;
    virtual InspectorTargetType type() const = 0;

    // Connection management.
    virtual void connect(FrontendChannel&) = 0;
    virtual void disconnect(FrontendChannel&) = 0;
    virtual void sendMessageToTargetBackend(const String&) = 0;
};

} // namespace Inspector

namespace WTF {

template<> struct EnumTraits<Inspector::InspectorTargetType> {
    using values = EnumValues<
        Inspector::InspectorTargetType,
        Inspector::InspectorTargetType::JavaScriptContext,
        Inspector::InspectorTargetType::Page,
        Inspector::InspectorTargetType::DedicatedWorker,
        Inspector::InspectorTargetType::ServiceWorker
    >;
};

} // namespace WTF
