/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef MediaSessionManager_h
#define MediaSessionManager_h

#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class MediaSessionManagerToken;

class MediaSessionManager {
public:
    static MediaSessionManager& sharedManager();

    enum MediaType {
        None,
        Video,
        Audio,
        WebAudio,
    };

    bool has(MediaType) const;
    int count(MediaType) const;

protected:
    friend class MediaSessionManagerToken;
    void addToken(MediaSessionManagerToken&);
    void removeToken(MediaSessionManagerToken&);

private:
    MediaSessionManager();

    void updateSessionState();

    Vector<MediaSessionManagerToken*> m_tokens;
};

class MediaSessionManagerClient {
    WTF_MAKE_NONCOPYABLE(MediaSessionManagerClient);
public:
    MediaSessionManagerClient() { }

    virtual MediaSessionManager::MediaType mediaType() const = 0;

protected:
    virtual ~MediaSessionManagerClient() { }
};

class MediaSessionManagerToken {
public:
    static std::unique_ptr<MediaSessionManagerToken> create(MediaSessionManagerClient&);

    MediaSessionManagerToken(MediaSessionManagerClient&);
    ~MediaSessionManagerToken();

    MediaSessionManager::MediaType mediaType() const { return m_type; }

private:

    MediaSessionManagerClient& m_client;
    MediaSessionManager::MediaType m_type;
};

}

#endif // MediaSessionManager_h
