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

} // namespace WebCore
