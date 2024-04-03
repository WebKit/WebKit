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
#include "WebKitNamespace.h"

#include "FrameLoader.h"
#include "LocalFrameLoaderClient.h"
#include "Logging.h"

#define WEBKIT_NAMESPACE_RELEASE_LOG_ERROR(channel, fmt, ...) RELEASE_LOG_ERROR(channel, "%p - WebKitNamespace::" fmt, this, ##__VA_ARGS__)

#if ENABLE(USER_MESSAGE_HANDLERS)

#include "LocalDOMWindow.h"
#include "UserMessageHandlersNamespace.h"

namespace WebCore {

WebKitNamespace::WebKitNamespace(LocalDOMWindow& window, UserContentProvider& userContentProvider)
    : LocalDOMWindowProperty(&window)
    , m_messageHandlerNamespace(UserMessageHandlersNamespace::create(*window.protectedFrame(), userContentProvider))
{
    ASSERT(window.frame());
}

WebKitNamespace::~WebKitNamespace() = default;

UserMessageHandlersNamespace* WebKitNamespace::messageHandlers()
{
#if ENABLE(APP_BOUND_DOMAINS)
    if (frame()) {
        if (frame()->loader().client().shouldEnableInAppBrowserPrivacyProtections()) {
            WEBKIT_NAMESPACE_RELEASE_LOG_ERROR(Loading, "Ignoring messageHandlers() request for non app-bound domain");
            return nullptr;
        }
        frame()->loader().client().notifyPageOfAppBoundBehavior();
    }
#endif

    return &m_messageHandlerNamespace.get();
}

} // namespace WebCore

#endif // ENABLE(USER_MESSAGE_HANDLERS)

#undef WEBKIT_NAMESPACE_RELEASE_LOG_ERROR
