/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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

#include "MediaEngineConfigurationFactory.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class DeferredPromise;
class ScriptExecutionContext;

class MediaCapabilities : public RefCounted<MediaCapabilities>, public CanMakeWeakPtr<MediaCapabilities> {
public:
    static Ref<MediaCapabilities> create() { return adoptRef(*new MediaCapabilities); }

    void decodingInfo(ScriptExecutionContext&, MediaDecodingConfiguration&&, Ref<DeferredPromise>&&);
    void encodingInfo(ScriptExecutionContext&, MediaEncodingConfiguration&&, Ref<DeferredPromise>&&);

private:
    MediaCapabilities() = default;

    uint64_t m_nextTaskIdentifier { 0 };
    HashMap<uint64_t, MediaEngineConfigurationFactory::DecodingConfigurationCallback> m_decodingTasks;
    HashMap<uint64_t, MediaEngineConfigurationFactory::EncodingConfigurationCallback> m_encodingTasks;
};

} // namespace WebCore
