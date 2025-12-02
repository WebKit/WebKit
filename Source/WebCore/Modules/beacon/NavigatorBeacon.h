/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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

#include "CachedRawResourceClient.h"
#include "CachedResourceHandle.h"
#include "FetchBody.h"
#include "Supplementable.h"
#include <wtf/CheckedRef.h>
#include <wtf/Forward.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class CachedRawResource;
class Document;
class Navigator;
class ResourceError;
template<typename> class ExceptionOr;

class NavigatorBeacon final : public Supplement<Navigator>, private CachedRawResourceClient {
    WTF_MAKE_TZONE_ALLOCATED(NavigatorBeacon);
public:
    explicit NavigatorBeacon(Navigator&);
    ~NavigatorBeacon();
    static ExceptionOr<bool> sendBeacon(Navigator&, Document&, const String& url, std::optional<FetchBody::Init>&&);

    size_t inflightBeaconsCount() const { return m_inflightBeacons.size(); }

    WEBCORE_EXPORT static Ref<NavigatorBeacon> from(Navigator&);

    // CachedResourceClient.
    WEBCORE_EXPORT void ref() const final;
    WEBCORE_EXPORT void deref() const final;

private:
    ExceptionOr<bool> sendBeacon(Document&, const String& url, std::optional<FetchBody::Init>&&);

    static ASCIILiteral supplementName() { return "NavigatorBeacon"_s; }
    bool isNavigatorBeacon() const final { return true; }

    void notifyFinished(CachedResource&, const NetworkLoadMetrics&, LoadWillContinueInAnotherProcess) final;
    void logError(const ResourceError&);

    const CheckedRef<Navigator> m_navigator;
    Vector<CachedResourceHandle<CachedRawResource>> m_inflightBeacons;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::NavigatorBeacon)
    static bool isType(const WebCore::SupplementBase& supplement) { return supplement.isNavigatorBeacon(); }
SPECIALIZE_TYPE_TRAITS_END()
