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

#include "ContentWorldShared.h"
#include "WebProcessMessages.h"
#include "WebProcessProxy.h"
#include "WebUserContentControllerProxy.h"
#include <wtf/HashMap.h>
#include <wtf/WeakRef.h>
#include <wtf/text/StringHash.h>

namespace API {

OptionSet<WebKit::ContentWorldOption> ContentWorld::defaultOptions()
{
    return WebKit::ContentWorldOption::Inspectable;
}

static HashMap<WTF::String, WeakRef<ContentWorld>>& NODELETE sharedWorldNameMap()
{
    static NeverDestroyed<HashMap<WTF::String, WeakRef<ContentWorld>>> sharedMap;
    return sharedMap;
}

static HashMap<WebKit::ContentWorldIdentifier, WeakRef<ContentWorld>>& NODELETE sharedWorldIdentifierMap()
{
    static NeverDestroyed<HashMap<WebKit::ContentWorldIdentifier, WeakRef<ContentWorld>>> sharedMap;
    return sharedMap;
}

ContentWorld* ContentWorld::worldForIdentifier(WebKit::ContentWorldIdentifier identifier)
{
    if (identifier == WebKit::pageContentWorldIdentifier())
        return &pageContentWorldSingleton();
    return sharedWorldIdentifierMap().get(identifier);
}

static WebKit::ContentWorldIdentifier generateIdentifier()
{
    static std::once_flag once;
    std::call_once(once, [] {
        // To make sure we don't use our shared pageContentWorld identifier for this
        // content world we're about to make, burn through one identifier.
        auto identifier = WebKit::ContentWorldIdentifier::generate();
        ASSERT_UNUSED(identifier, identifier.object().toUInt64() >= WebKit::pageContentWorldIdentifier().object().toUInt64());
    });
    return WebKit::ContentWorldIdentifier::generate();
}

ContentWorld::ContentWorld(const WTF::String& name, OptionSet<WebKit::ContentWorldOption> options)
    : m_identifier(generateIdentifier())
    , m_name(name)
    , m_options(options)
{
    auto addResult = sharedWorldIdentifierMap().add(m_identifier, *this);
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
}

ContentWorld::ContentWorld(WebKit::ContentWorldIdentifier identifier)
    : m_identifier(identifier)
    , m_options(defaultOptions())
{
    ASSERT(m_identifier == WebKit::pageContentWorldIdentifier());
}

Ref<ContentWorld> ContentWorld::sharedWorldWithName(const WTF::String& name, OptionSet<WebKit::ContentWorldOption> options)
{
    RefPtr<ContentWorld> newContentWorld;
    auto result = sharedWorldNameMap().ensure(name, [&] {
        newContentWorld = adoptRef(*new ContentWorld(name, options));
        return WeakRef { *newContentWorld };
    });
    return newContentWorld ? newContentWorld.releaseNonNull() : Ref { result.iterator->value.get() };
}

Ref<ContentWorld> ContentWorld::createNamelessWorld(OptionSet<WebKit::ContentWorldOption> options)
{
    return adoptRef(*new ContentWorld(nullString(), options));
}

ContentWorld& ContentWorld::pageContentWorldSingleton()
{
    static NeverDestroyed<Ref<ContentWorld>> world(adoptRef(*new ContentWorld(WebKit::pageContentWorldIdentifier())));
    return world.get();
}

ContentWorld& ContentWorld::defaultClientWorldSingleton()
{
    static NeverDestroyed<Ref<ContentWorld>> world(adoptRef(*new ContentWorld(WTF::String { }, defaultOptions())));
    return world.get();
}

ContentWorld::~ContentWorld()
{
    ASSERT(m_identifier != WebKit::pageContentWorldIdentifier());

    auto result = sharedWorldIdentifierMap().take(m_identifier);
    ASSERT_UNUSED(result, result.get() == this || m_identifier == WebKit::pageContentWorldIdentifier());

    if (!name().isNull()) {
        auto taken = sharedWorldNameMap().take(name());
        ASSERT_UNUSED(taken, taken.get() == this);
    }

    for (Ref process : m_processes)
        process->send(Messages::WebProcess::ContentWorldDestroyed(m_identifier), 0);
}

WebKit::ContentWorldData ContentWorld::worldDataForProcess(WebKit::WebProcessProxy& process) const
{
    m_processes.add(process);
    return { m_identifier, m_name, m_options };
}

} // namespace API
