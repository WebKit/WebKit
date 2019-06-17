/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "UserMessageHandlersNamespace.h"

#if ENABLE(USER_MESSAGE_HANDLERS)

#include "DOMWrapperWorld.h"
#include "Frame.h"
#include "Page.h"
#include "UserContentController.h"
#include "UserMessageHandler.h"

namespace WebCore {

UserMessageHandlersNamespace::UserMessageHandlersNamespace(Frame& frame, UserContentProvider& userContentProvider)
    : FrameDestructionObserver(&frame)
    , m_userContentProvider(userContentProvider)
{
    m_userContentProvider->registerForUserMessageHandlerInvalidation(*this);
}

UserMessageHandlersNamespace::~UserMessageHandlersNamespace()
{
    m_userContentProvider->unregisterForUserMessageHandlerInvalidation(*this);
}

void UserMessageHandlersNamespace::didInvalidate(UserContentProvider& provider)
{
    auto oldMap = WTFMove(m_messageHandlers);

    provider.forEachUserMessageHandler([&](const UserMessageHandlerDescriptor& descriptor) {
        auto userMessageHandler = oldMap.take(std::make_pair(descriptor.name(), const_cast<DOMWrapperWorld*>(&descriptor.world())));
        if (userMessageHandler) {
            m_messageHandlers.add(std::make_pair(descriptor.name(), const_cast<DOMWrapperWorld*>(&descriptor.world())), userMessageHandler);
            return;
        }
    });

    for (auto& userMessageHandler : oldMap.values())
        userMessageHandler->invalidateDescriptor();
}

Vector<AtomString> UserMessageHandlersNamespace::supportedPropertyNames() const
{
    // FIXME: Consider adding support for iterating the registered UserMessageHandlers. This would
    // require adding support for passing the DOMWrapperWorld to supportedPropertyNames.
    return { };
}

UserMessageHandler* UserMessageHandlersNamespace::namedItem(DOMWrapperWorld& world, const AtomString& name)
{
    Frame* frame = this->frame();
    if (!frame)
        return nullptr;

    Page* page = frame->page();
    if (!page)
        return nullptr;

    UserMessageHandler* handler = m_messageHandlers.get(std::pair<AtomString, RefPtr<DOMWrapperWorld>>(name, &world));
    if (handler)
        return handler;

    page->userContentProvider().forEachUserMessageHandler([&](const UserMessageHandlerDescriptor& descriptor) {
        if (descriptor.name() != name || &descriptor.world() != &world)
            return;
        
        ASSERT(!handler);

        auto addResult = m_messageHandlers.add(std::make_pair(descriptor.name(), const_cast<DOMWrapperWorld*>(&descriptor.world())), UserMessageHandler::create(*frame, const_cast<UserMessageHandlerDescriptor&>(descriptor)));
        handler = addResult.iterator->value.get();
    });

    return handler;
}

} // namespace WebCore

#endif // ENABLE(USER_MESSAGE_HANDLERS)
