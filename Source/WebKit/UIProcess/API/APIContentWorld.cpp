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

#include "config.h"
#include "APIContentWorld.h"

#include "APIUserContentWorld.h"
#include "ContentWorldShared.h"
#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>

namespace API {

ContentWorldBase::ContentWorldBase(const WTF::String& name)
    : m_name(name)
{
    static std::once_flag once;
    std::call_once(once, [] {
        // To make sure we don't use our shared pageContentWorld identifier for this
        // content world we're about to make, burn through one identifier.
        auto identifier = WebKit::ContentWorldIdentifier::generate();
        ASSERT_UNUSED(identifier, identifier.toUInt64() >= WebKit::pageContentWorldIdentifier().toUInt64());
    });

    m_identifier = WebKit::ContentWorldIdentifier::generate();
}

static HashMap<WTF::String, ContentWorld*>& sharedWorldMap()
{
    static HashMap<WTF::String, ContentWorld*>* sharedMap = new HashMap<WTF::String, ContentWorld*>;
    return *sharedMap;
}

Ref<ContentWorld> ContentWorld::sharedWorldWithName(const WTF::String& name)
{
    auto result = sharedWorldMap().add(name, nullptr);
    if (result.isNewEntry) {
        result.iterator->value = new ContentWorld(name);
        return adoptRef(*result.iterator->value);
    }

    return makeRef(*result.iterator->value);
}

ContentWorld& ContentWorld::pageContentWorld()
{
    static NeverDestroyed<RefPtr<ContentWorld>> world(adoptRef(new ContentWorld(WebKit::pageContentWorldIdentifier())));
    return *world.get();
}

ContentWorld& ContentWorld::defaultClientWorld()
{
    static NeverDestroyed<RefPtr<ContentWorld>> world(adoptRef(new ContentWorld(WTF::String { })));
    return *world.get();
}

ContentWorld::ContentWorld(const WTF::String& name)
    : ContentWorldBase(name)
{
}

ContentWorld::ContentWorld(WebKit::ContentWorldIdentifier identifier)
    : ContentWorldBase(identifier)
{
}

ContentWorld::~ContentWorld()
{
    if (name().isNull())
        return;

    auto taken = sharedWorldMap().take(name());
    ASSERT_UNUSED(taken, taken == this);
}

} // namespace API
