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

#include "config.h"
#include "MediaSessionManager.h"

using namespace WebCore;


std::unique_ptr<MediaSessionManagerToken> MediaSessionManagerToken::create(MediaSessionManagerClient& client)
{
    return std::make_unique<MediaSessionManagerToken>(client);
}

MediaSessionManagerToken::MediaSessionManagerToken(MediaSessionManagerClient& client)
    : m_client(client)
{
    m_type = m_client.mediaType();
    MediaSessionManager::sharedManager().addToken(*this);
}

MediaSessionManagerToken::~MediaSessionManagerToken()
{
    MediaSessionManager::sharedManager().removeToken(*this);
}

MediaSessionManager& MediaSessionManager::sharedManager()
{
    DEFINE_STATIC_LOCAL(MediaSessionManager, manager, ());
    return manager;
}

MediaSessionManager::MediaSessionManager()
{
}

bool MediaSessionManager::has(MediaSessionManager::MediaType type) const
{
    ASSERT(type >= MediaSessionManager::None && type <= MediaSessionManager::WebAudio);

    for (auto it = m_tokens.begin(), end = m_tokens.end(); it != end; ++it) {
        if ((*it)->mediaType() == type)
            return true;
    }
    
    return false;
}

int MediaSessionManager::count(MediaSessionManager::MediaType type) const
{
    ASSERT(type >= MediaSessionManager::None && type <= MediaSessionManager::WebAudio);
    
    int count = 0;
    for (auto it = m_tokens.begin(), end = m_tokens.end(); it != end; ++it) {
        if ((*it)->mediaType() == type)
            ++count;
    }
    
    return count;
}

void MediaSessionManager::addToken(MediaSessionManagerToken& token)
{
    m_tokens.append(&token);
    updateSessionState();
}

void MediaSessionManager::removeToken(MediaSessionManagerToken& token)
{
    size_t index = m_tokens.find(&token);
    ASSERT(index != notFound);
    if (index == notFound)
        return;

    m_tokens.remove(index);
    updateSessionState();
}

#if !PLATFORM(MAC)
void MediaSessionManager::updateSessionState()
{
}
#endif
