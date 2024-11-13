/*
 * Copyright (C) 2018, 2020 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
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

#include "DNSResolveQueue.h"

#if USE(GLIB)

#include <wtf/CompletionHandler.h>
#include <wtf/HashMap.h>
#include <wtf/glib/GRefPtr.h>

namespace WebCore {

class DNSResolveQueueGLib final : public DNSResolveQueue {
public:
    DNSResolveQueueGLib() = default;

    void resolve(const String& hostname, uint64_t identifier, DNSCompletionHandler&&) final;
    void stopResolve(uint64_t identifier) final;

private:
    struct Request {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;

        Request(uint64_t identifier, DNSCompletionHandler&& completionHandler)
            : identifier(identifier)
            , completionHandler(WTFMove(completionHandler))
        {
        }

        uint64_t identifier { 0 };
        DNSCompletionHandler completionHandler;
    };

    void updateIsUsingProxy() final;
    void platformResolve(const String&) final;

    HashMap<uint64_t, GRefPtr<GCancellable>> m_requestCancellables;
};

using DNSResolveQueuePlatform = DNSResolveQueueGLib;

} // namespace WebCore

#endif // USE(GLIB)
