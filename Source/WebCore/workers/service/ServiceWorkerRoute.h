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

#include "ExceptionData.h"
#include "FetchRequestDestination.h"
#include "FetchRequestMode.h"
#include "RouterSourceDict.h"
#include "RouterSourceEnum.h"
#include "RunningStatus.h"
#include <optional>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ResourceRequest;
struct FetchOptions;

struct ServiceWorkerRoutePattern {
    ServiceWorkerRoutePattern isolatedCopy() &&;

    using Component = String;

    Component protocol;
    Component username;
    Component password;
    Component hostname;
    Component port;
    Component pathname;
    Component search;
    Component hash;
};

struct ServiceWorkerRouteCondition {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(ServiceWorkerRouteCondition);

    ServiceWorkerRouteCondition isolatedCopy() &&;
    ServiceWorkerRouteCondition copy() const;

    std::optional<ServiceWorkerRoutePattern> urlPattern;
    String requestMethod;
    std::optional<FetchRequestMode> requestMode;
    std::optional<FetchRequestDestination> requestDestination;
    std::optional<RunningStatus> runningStatus;

    Vector<ServiceWorkerRouteCondition> orConditions;
    std::unique_ptr<ServiceWorkerRouteCondition> notCondition;
};

using RouterSource = Variant<RouterSourceDict, RouterSourceEnum>;

struct ServiceWorkerRoute {
    ServiceWorkerRouteCondition condition;
    RouterSource source;

    ServiceWorkerRoute copy() const { return { condition.copy(), source }; }
    ServiceWorkerRoute isolatedCopy() &&;
};

size_t computeServiceWorkerRouteConditionCount(const ServiceWorkerRoute&);
std::optional<ExceptionData> validateServiceWorkerRoute(ServiceWorkerRoute&);
bool matchRouterCondition(const ServiceWorkerRouteCondition&, const FetchOptions&, const ResourceRequest&, bool isServiceWorkerRunning);

} // namespace WebCore
