/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#pragma once

#include <WebCore/NetworkInterception.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class WebPage;

// Receives the network interception configuration that the UIProcess pushes down
// to each WebContent process hosting a frame of the page, and holds the copy that
// is active in this process. Under Site Isolation a page's frames are spread over
// several processes, so every hosting process needs its own copy: the UIProcess
// broadcasts on each configuration write and replays into late-joining processes.
//
// This only stores configuration. Pausing loads against it is a separate change.
class NetworkInterceptionSink final {
    WTF_MAKE_TZONE_ALLOCATED(NetworkInterceptionSink);
    WTF_MAKE_NONCOPYABLE(NetworkInterceptionSink);
public:
    explicit NetworkInterceptionSink(WebPage&);
    ~NetworkInterceptionSink();

    void setConfiguration(Inspector::NetworkInterception&&);
    const Inspector::NetworkInterception& configuration() const { return m_configuration; }

private:
    WeakRef<WebPage> m_page;
    Inspector::NetworkInterception m_configuration;
};

} // namespace WebKit
