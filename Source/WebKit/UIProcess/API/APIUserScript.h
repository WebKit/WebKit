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

#include "APIObject.h"
#include "APIUserContentWorld.h"
#include <WebCore/UserScript.h>
#include <wtf/Identified.h>

namespace API {

class UserScript final : public ObjectImpl<Object::Type::UserScript>, public Identified<UserScript> {
public:
    static WTF::URL generateUniqueURL();

    static Ref<UserScript> create(WebCore::UserScript userScript, API::UserContentWorld& world)
    {
        return adoptRef(*new UserScript(WTFMove(userScript), world));
    }

    UserScript(WebCore::UserScript, API::UserContentWorld&);

    const WebCore::UserScript& userScript() const { return m_userScript; }
    
    UserContentWorld& userContentWorld() { return m_world; }
    const UserContentWorld& userContentWorld() const { return m_world; }
    
private:
    WebCore::UserScript m_userScript;
    Ref<UserContentWorld> m_world;
};

} // namespace API
