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

#ifndef FetchOptions_h
#define FetchOptions_h

#if ENABLE(FETCH_API)

namespace WebCore {

class FetchOptions {
public:
    enum class Type {
        Default,
        Audio,
        Font,
        Image,
        Script,
        Style,
        Track,
        Video
    };
    enum class Destination {
        Default,
        Document,
        SharedWorker,
        Subresource,
        Unknown,
        Worker
    };
    enum class Mode {
        NoCors,
        Navigate,
        SameOrigin,
        Cors
    };
    enum class Credentials {
        Omit,
        SameOrigin,
        Include
    };
    enum class Cache {
        Default,
        NoStore,
        Reload,
        NoCache,
        ForceCache,
    };
    enum class Redirect {
        Follow,
        Error,
        Manual
    };
    enum class ReferrerPolicy {
        Empty,
        NoReferrer,
        NoReferrerWhenDowngrade,
        OriginOnly,
        OriginWhenCrossOrigin,
        UnsafeURL
    };

    FetchOptions() { }
    FetchOptions(Type, Destination, Mode, Credentials, Cache, Redirect, ReferrerPolicy);

    Type type() const { return m_type; }
    Destination destination() const { return m_destination; }
    Mode mode() const { return m_mode; }
    Credentials credentials() const { return m_credentials; }
    Cache cache() const { return m_cache; }
    Redirect redirect() const { return m_redirect; }
    ReferrerPolicy referrerPolicy() const { return m_referrerPolicy; }

    void setType(Type type) { m_type = type; }
    void setDestination(Destination destination) { m_destination = destination; }
    void setMode(Mode mode) { m_mode = mode; }
    void setCredentials(Credentials credentials) { m_credentials = credentials; }
    void setCache(Cache cache) { m_cache = cache; }
    void setRedirect(Redirect redirect) { m_redirect = redirect; }
    void setReferrerPolicy(ReferrerPolicy referrerPolicy) { m_referrerPolicy = referrerPolicy; }

private:
    Type m_type = Type::Default;
    Destination m_destination = Destination::Default;
    Mode m_mode = Mode::NoCors;
    Credentials m_credentials = Credentials::Omit;
    Cache m_cache = Cache::Default;
    Redirect m_redirect = Redirect:: Follow;
    ReferrerPolicy m_referrerPolicy = ReferrerPolicy::Empty;
};

inline FetchOptions::FetchOptions(Type type, Destination destination, Mode mode, Credentials credentials, Cache cache, Redirect redirect, ReferrerPolicy referrerPolicy)
    : m_type(type)
    , m_destination(destination)
    , m_mode(mode)
    , m_credentials(credentials)
    , m_cache(cache)
    , m_redirect(redirect)
    , m_referrerPolicy(referrerPolicy)
{
}

} // namespace WebCore

#endif // ENABLE(FETCH_API)

#endif // FetchOptions_h
