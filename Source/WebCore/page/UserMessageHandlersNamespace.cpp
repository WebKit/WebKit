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

namespace WebCore {

UserMessageHandlersNamespace::UserMessageHandlersNamespace(Frame& frame)
    : FrameDestructionObserver(&frame)
{
}

UserMessageHandlersNamespace::~UserMessageHandlersNamespace()
{
}

UserMessageHandler* UserMessageHandlersNamespace::handler(const AtomicString& name, DOMWrapperWorld& world)
{
    // First, check if we have a handler instance already.
    for (auto& handler : m_messageHandlers) {
        if (handler->name() == name && &handler->world() == &world)
            return &handler.get();
    }

    // Second, attempt to create a handler instance from a descriptor.
    if (!frame())
        return nullptr;

    Page* page = frame()->page();
    if (!page)
        return nullptr;
    
    const auto* userContentController = page->userContentController();
    if (!userContentController)
        return nullptr;

    const auto* userMessageHandlerDescriptors = userContentController->userMessageHandlerDescriptors();
    if (!userMessageHandlerDescriptors)
        return nullptr;

    RefPtr<UserMessageHandlerDescriptor> descriptor = userMessageHandlerDescriptors->get(std::make_pair(name, &world));
    if (!descriptor)
        return nullptr;

    m_messageHandlers.append(UserMessageHandler::create(*frame(), *descriptor));
    return &m_messageHandlers.last().get();
}

} // namespace WebCore

#endif // ENABLE(USER_MESSAGE_HANDLERS)
