/*
 * Copyright (C) 2016 Canon Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Canon Inc. nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CANON INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CANON INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ReferrerPolicy.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

struct FetchOptions {
    enum class Type { EmptyString, Audio, Font, Image, Script, Style, Track, Video };
    enum class Destination { EmptyString, Document, Sharedworker, Subresource, Unknown, Worker };
    enum class Mode { Navigate, SameOrigin, NoCors, Cors };
    enum class Credentials { Omit, SameOrigin, Include };
    enum class Cache { Default, NoStore, Reload, NoCache, ForceCache, OnlyIfCached };
    enum class Redirect { Follow, Error, Manual };

    FetchOptions() = default;
    FetchOptions(Type, Destination, Mode, Credentials, Cache, Redirect, ReferrerPolicy, String&&, bool);
    FetchOptions isolatedCopy() const { return { type, destination, mode, credentials, cache, redirect, referrerPolicy, integrity.isolatedCopy(), keepAlive }; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, FetchOptions&);

    Type type { Type::EmptyString };
    Destination destination { Destination::EmptyString };
    Mode mode { Mode::NoCors };
    Credentials credentials { Credentials::Omit };
    Cache cache { Cache::Default };
    Redirect redirect { Redirect::Follow };
    ReferrerPolicy referrerPolicy { ReferrerPolicy::EmptyString };
    String integrity;
    bool keepAlive { false };
};

inline FetchOptions::FetchOptions(Type type, Destination destination, Mode mode, Credentials credentials, Cache cache, Redirect redirect, ReferrerPolicy referrerPolicy, String&& integrity, bool keepAlive)
    : type(type)
    , destination(destination)
    , mode(mode)
    , credentials(credentials)
    , cache(cache)
    , redirect(redirect)
    , referrerPolicy(referrerPolicy)
    , integrity(WTFMove(integrity))
    , keepAlive(keepAlive)
{
}

}

namespace WTF {

template<> struct EnumTraits<WebCore::FetchOptions::Type> {
    using values = EnumValues<
        WebCore::FetchOptions::Type,
        WebCore::FetchOptions::Type::EmptyString,
        WebCore::FetchOptions::Type::Audio,
        WebCore::FetchOptions::Type::Font,
        WebCore::FetchOptions::Type::Image,
        WebCore::FetchOptions::Type::Script,
        WebCore::FetchOptions::Type::Style,
        WebCore::FetchOptions::Type::Track,
        WebCore::FetchOptions::Type::Video
    >;
};

template<> struct EnumTraits<WebCore::FetchOptions::Destination> {
    using values = EnumValues<
        WebCore::FetchOptions::Destination,
        WebCore::FetchOptions::Destination::EmptyString,
        WebCore::FetchOptions::Destination::Document,
        WebCore::FetchOptions::Destination::Sharedworker,
        WebCore::FetchOptions::Destination::Subresource,
        WebCore::FetchOptions::Destination::Unknown,
        WebCore::FetchOptions::Destination::Worker
    >;
};

template<> struct EnumTraits<WebCore::FetchOptions::Mode> {
    using values = EnumValues<
        WebCore::FetchOptions::Mode,
        WebCore::FetchOptions::Mode::Navigate,
        WebCore::FetchOptions::Mode::SameOrigin,
        WebCore::FetchOptions::Mode::NoCors,
        WebCore::FetchOptions::Mode::Cors
    >;
};

template<> struct EnumTraits<WebCore::FetchOptions::Credentials> {
    using values = EnumValues<
        WebCore::FetchOptions::Credentials,
        WebCore::FetchOptions::Credentials::Omit,
        WebCore::FetchOptions::Credentials::SameOrigin,
        WebCore::FetchOptions::Credentials::Include
    >;
};

template<> struct EnumTraits<WebCore::FetchOptions::Cache> {
    using values = EnumValues<
        WebCore::FetchOptions::Cache,
        WebCore::FetchOptions::Cache::Default,
        WebCore::FetchOptions::Cache::NoStore,
        WebCore::FetchOptions::Cache::Reload,
        WebCore::FetchOptions::Cache::NoCache,
        WebCore::FetchOptions::Cache::ForceCache,
        WebCore::FetchOptions::Cache::OnlyIfCached
    >;
};

template<> struct EnumTraits<WebCore::FetchOptions::Redirect> {
    using values = EnumValues<
        WebCore::FetchOptions::Redirect,
        WebCore::FetchOptions::Redirect::Follow,
        WebCore::FetchOptions::Redirect::Error,
        WebCore::FetchOptions::Redirect::Manual
    >;
};

}

namespace WebCore {

template<class Encoder> inline void FetchOptions::encode(Encoder& encoder) const
{
    encoder << type;
    encoder << destination;
    encoder << mode;
    encoder << credentials;
    encoder << cache;
    encoder << redirect;
    encoder << referrerPolicy;
    encoder << integrity;
    encoder << keepAlive;
}

template<class Decoder> inline bool FetchOptions::decode(Decoder& decoder, FetchOptions& options)
{
    FetchOptions::Type type;
    if (!decoder.decode(type))
        return false;

    FetchOptions::Destination destination;
    if (!decoder.decode(destination))
        return false;

    FetchOptions::Mode mode;
    if (!decoder.decode(mode))
        return false;

    FetchOptions::Credentials credentials;
    if (!decoder.decode(credentials))
        return false;

    FetchOptions::Cache cache;
    if (!decoder.decode(cache))
        return false;

    FetchOptions::Redirect redirect;
    if (!decoder.decode(redirect))
        return false;

    ReferrerPolicy referrerPolicy;
    if (!decoder.decode(referrerPolicy))
        return false;

    String integrity;
    if (!decoder.decode(integrity))
        return false;

    bool keepAlive;
    if (!decoder.decode(keepAlive))
        return false;

    options.type = type;
    options.destination = destination;
    options.mode = mode;
    options.credentials = credentials;
    options.cache = cache;
    options.redirect = redirect;
    options.referrerPolicy = referrerPolicy;
    options.integrity = WTFMove(integrity);
    options.keepAlive = keepAlive;

    return true;
}

} // namespace WebCore
