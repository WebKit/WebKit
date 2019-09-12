/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if ENABLE(SERVICE_WORKER)

#include "DocumentIdentifier.h"
#include "ProcessIdentifier.h"
#include "ServiceWorkerIdentifier.h"
#include <wtf/ObjectIdentifier.h>
#include <wtf/Variant.h>

namespace WebCore {

struct ServiceWorkerData;
struct ServiceWorkerClientData;
struct ServiceWorkerClientIdentifier;

enum class ServiceWorkerRegistrationState : uint8_t {
    Installing = 0,
    Waiting = 1,
    Active = 2,
};

enum class ServiceWorkerState : uint8_t {
    Installing,
    Installed,
    Activating,
    Activated,
    Redundant,
};

enum class ServiceWorkerClientFrameType {
    Auxiliary,
    TopLevel,
    Nested,
    None
};

enum class ShouldNotifyWhenResolved : bool { No, Yes };

enum ServiceWorkerRegistrationIdentifierType { };
using ServiceWorkerRegistrationIdentifier = ObjectIdentifier<ServiceWorkerRegistrationIdentifierType>;

enum ServiceWorkerJobIdentifierType { };
using ServiceWorkerJobIdentifier = ObjectIdentifier<ServiceWorkerJobIdentifierType>;

enum SWServerToContextConnectionIdentifierType { };
using SWServerToContextConnectionIdentifier = ObjectIdentifier<SWServerToContextConnectionIdentifierType>;

using SWServerConnectionIdentifierType = ProcessIdentifierType;
using SWServerConnectionIdentifier = ObjectIdentifier<SWServerConnectionIdentifierType>;

using DocumentOrWorkerIdentifier = Variant<DocumentIdentifier, ServiceWorkerIdentifier>;

using ServiceWorkerOrClientData = Variant<ServiceWorkerData, ServiceWorkerClientData>;
using ServiceWorkerOrClientIdentifier = Variant<ServiceWorkerIdentifier, ServiceWorkerClientIdentifier>;

} // namespace WebCore

namespace WTF {

template <> struct EnumTraits<WebCore::ServiceWorkerClientFrameType> {
    using values = EnumValues<
        WebCore::ServiceWorkerClientFrameType,
        WebCore::ServiceWorkerClientFrameType::Auxiliary,
        WebCore::ServiceWorkerClientFrameType::TopLevel,
        WebCore::ServiceWorkerClientFrameType::Nested,
        WebCore::ServiceWorkerClientFrameType::None
    >;
};

template <> struct EnumTraits<WebCore::ServiceWorkerRegistrationState> {
    using values = EnumValues<
        WebCore::ServiceWorkerRegistrationState,
        WebCore::ServiceWorkerRegistrationState::Installing,
        WebCore::ServiceWorkerRegistrationState::Waiting,
        WebCore::ServiceWorkerRegistrationState::Active
    >;
};

template <> struct EnumTraits<WebCore::ServiceWorkerState> {
    using values = EnumValues<
        WebCore::ServiceWorkerState,
        WebCore::ServiceWorkerState::Installing,
        WebCore::ServiceWorkerState::Installed,
        WebCore::ServiceWorkerState::Activating,
        WebCore::ServiceWorkerState::Activated,
        WebCore::ServiceWorkerState::Redundant
    >;
};

template <> struct EnumTraits<WebCore::ShouldNotifyWhenResolved> {
    using values = EnumValues<
        WebCore::ShouldNotifyWhenResolved,
        WebCore::ShouldNotifyWhenResolved::No,
        WebCore::ShouldNotifyWhenResolved::Yes
    >;
};

} // namespace WTF

#endif // ENABLE(SERVICE_WORKER)
