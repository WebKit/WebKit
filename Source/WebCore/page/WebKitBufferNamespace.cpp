/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "WebKitBufferNamespace.h"

#include "LocalFrame.h"
#include "UserContentProvider.h"

namespace WebCore {

WebKitBufferNamespace::WebKitBufferNamespace(LocalFrame& frame, UserContentProvider& provider)
    : m_userContentProvider(provider)
    , m_frame(frame) { }

WebKitBufferNamespace::~WebKitBufferNamespace() = default;

WebKitBuffer* WebKitBufferNamespace::namedItem(DOMWrapperWorld& world, const AtomString& name)
{
    RefPtr frame = m_frame.get();
    if (!frame)
        return nullptr;
    RefPtr provider = frame->userContentProvider();
    if (!provider)
        return nullptr;
    return provider->buffer(world, String(name));
}

Vector<AtomString> WebKitBufferNamespace::supportedPropertyNames() const
{
    // Prevent JS from iterating available matchers.
    return { };
}

bool WebKitBufferNamespace::isSupportedPropertyName(const AtomString&)
{
    // Prevent JS from iterating available matchers.
    return false;
}

} // namespace WebCore
