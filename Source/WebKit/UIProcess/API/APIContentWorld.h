/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "ContentWorldShared.h"
#include <wtf/text/WTFString.h>

namespace API {

class ContentWorldBase {
public:
    virtual ~ContentWorldBase() = default;

    WebKit::ContentWorldIdentifier identifier() const { return m_identifier; }
    const WTF::String& name() const { return m_name; }
    std::pair<WebKit::ContentWorldIdentifier, WTF::String> worldData() const { return { m_identifier, m_name }; }

    virtual void ref() const = 0;
    virtual void deref() const = 0;

protected:
    ContentWorldBase(const WTF::String& name);
    ContentWorldBase(WebKit::ContentWorldIdentifier identifier)
        : m_identifier(identifier)
    {
    }

private:
    WebKit::ContentWorldIdentifier m_identifier;
    WTF::String m_name;
};

class ContentWorld final : public API::ObjectImpl<API::Object::Type::ContentWorld>, public ContentWorldBase {
public:
    static Ref<ContentWorld> sharedWorldWithName(const WTF::String&);
    static ContentWorld& pageContentWorld();
    static ContentWorld& defaultClientWorld();

    virtual ~ContentWorld();

    void ref() const final { ObjectImpl::ref(); }
    void deref() const final { ObjectImpl::deref(); }

private:
    explicit ContentWorld(const WTF::String&);
    explicit ContentWorld(WebKit::ContentWorldIdentifier);
};

} // namespace API
