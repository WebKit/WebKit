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
#include "UserMessageHandler.h"

#if ENABLE(USER_MESSAGE_HANDLERS)

#include "Frame.h"
#include "SerializedScriptValue.h"

namespace WebCore {

UserMessageHandler::UserMessageHandler(Frame& frame, UserMessageHandlerDescriptor& descriptor)
    : FrameDestructionObserver(&frame)
    , m_descriptor(&descriptor)
{
}

UserMessageHandler::~UserMessageHandler() = default;

ExceptionOr<void> UserMessageHandler::postMessage(RefPtr<SerializedScriptValue>&& value)
{
    // Check to see if the descriptor has been removed. This can happen if the host application has
    // removed the named message handler at the WebKit2 API level.
    if (!m_descriptor)
        return Exception { InvalidAccessError };

    m_descriptor->didPostMessage(*this, value.get());
    return { };
}

} // namespace WebCore

#endif // ENABLE(USER_MESSAGE_HANDLERS)
