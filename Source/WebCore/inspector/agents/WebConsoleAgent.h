/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "InspectorWebAgentBase.h"
#include <JavaScriptCore/InspectorConsoleAgent.h>

namespace WebCore {

class DOMWindow;
class ResourceError;
class ResourceResponse;
struct WebAgentContext;
typedef String ErrorString;

class WebConsoleAgent : public Inspector::InspectorConsoleAgent {
    WTF_MAKE_NONCOPYABLE(WebConsoleAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    WebConsoleAgent(WebAgentContext&);
    virtual ~WebConsoleAgent();

    // InspectorInstrumentation
    void frameWindowDiscarded(DOMWindow*);
    void didReceiveResponse(unsigned long requestIdentifier, const ResourceResponse&);
    void didFailLoading(unsigned long requestIdentifier, const ResourceError&);

protected:
};

} // namespace WebCore
