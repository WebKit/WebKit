/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA)
#include "WebSocketTaskCocoa.h"
#elif USE(SOUP)
#include "WebSocketTaskSoup.h"
#elif USE(CURL)
#include "WebSocketTaskCurl.h"
#else
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
class WebSocketTask;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebKit::WebSocketTask> : std::true_type { };
}

namespace WebKit {

struct SessionSet;

class WebSocketTask : public CanMakeWeakPtr<WebSocketTask> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(WebSocketTask);
public:
    typedef uint64_t TaskIdentifier;

    void sendString(std::span<const uint8_t>, CompletionHandler<void()>&&) { }
    void sendData(std::span<const uint8_t>, CompletionHandler<void()>&&) { }
    void close(int32_t code, const String& reason) { }

    void cancel() { }
    void resume() { }
    
    SessionSet* sessionSet() { return nullptr; }
};

} // namespace WebKit

#endif
