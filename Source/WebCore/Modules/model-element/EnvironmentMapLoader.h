/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(MODEL_ELEMENT_ENVIRONMENT_MAP)

#include "CachedRawResource.h"
#include "CachedRawResourceClient.h"
#include "CachedResourceHandle.h"
#include "SharedBuffer.h"
#include <wtf/Function.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/URL.h>

namespace WebCore {

class Element;

#if ENABLE(SPATIAL_PORTAL)
namespace Style {
struct EnvironmentMap;
}

std::optional<URL> resolvedEnvironmentMapURL(const Element&, const Style::EnvironmentMap&);
#endif

class EnvironmentMapLoader final : public RefCounted<EnvironmentMapLoader>, public CachedRawResourceClient {
    WTF_MAKE_TZONE_ALLOCATED(EnvironmentMapLoader);
public:
    using LoadCompletionHandler = Function<void(RefPtr<SharedBuffer>&&)>;

    static Ref<EnvironmentMapLoader> create() { return adoptRef(*new EnvironmentMapLoader()); }
    ~EnvironmentMapLoader();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    void load(Element&, const URL&, LoadCompletionHandler&&);
    void cancel();

private:
    EnvironmentMapLoader() = default;

    void dataReceived(CachedResource&, const SharedBuffer&) final;
    void notifyFinished(CachedResource&, const NetworkLoadMetrics&, LoadWillContinueInAnotherProcess) final;

    void clearResource();
    void complete(RefPtr<SharedBuffer>&&);

    SharedBufferBuilder m_data;
    CachedResourceHandle<CachedRawResource> m_resource;
    LoadCompletionHandler m_completionHandler;
};

} // namespace WebCore

#endif // ENABLE(MODEL_ELEMENT_ENVIRONMENT_MAP)
