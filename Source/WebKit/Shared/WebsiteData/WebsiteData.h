/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include <WebCore/RegistrableDomain.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/HashSet.h>
#include <wtf/OptionSet.h>
#include <wtf/Vector.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebKit {

enum class WebsiteDataType : uint32_t;

enum class WebsiteDataProcessType { Network, UI, Web };

struct WebsiteData {
    struct Entry {
        WebCore::SecurityOriginData origin;
        WebsiteDataType type;
        uint64_t size;

        Entry isolatedCopy() const &;
        Entry isolatedCopy() &&;

        void encode(IPC::Encoder&) const;
        static std::optional<WebsiteData::Entry> decode(IPC::Decoder&);
    };

    WebsiteData isolatedCopy() const &;
    WebsiteData isolatedCopy() &&;

    Vector<Entry> entries;
    HashSet<String> hostNamesWithCookies;

    HashSet<String> hostNamesWithHSTSCache;
#if ENABLE(TRACKING_PREVENTION)
    HashSet<WebCore::RegistrableDomain> registrableDomainsWithResourceLoadStatistics;
#endif

    void encode(IPC::Encoder&) const;
    static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, WebsiteData&);
    static WebsiteDataProcessType ownerProcess(WebsiteDataType);
    static OptionSet<WebsiteDataType> filter(OptionSet<WebsiteDataType>, WebsiteDataProcessType);
};

}
