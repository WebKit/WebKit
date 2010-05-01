/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "ScriptReturnValueCallback.h"

#include "WKAPICast.h"
#include <WebCore/PlatformString.h>

namespace WebKit {

static uint64_t generateCallbackID()
{
    static uint64_t uniqueCallbackID = 1;
    return uniqueCallbackID++;
}

ScriptReturnValueCallback::ScriptReturnValueCallback(void* context, ScriptReturnValueCallbackFunction callback, ScriptReturnValueCallbackDisposeFunction disposeCallback)
    : m_context(context)
    , m_callback(callback)
    , m_disposeCallback(disposeCallback)
    , m_callbackID(generateCallbackID())
{
}

ScriptReturnValueCallback::~ScriptReturnValueCallback()
{
    ASSERT(!m_callback);
}

void ScriptReturnValueCallback::performCallbackWithReturnValue(const WebCore::String& returnValue)
{
    ASSERT(m_callback);

    m_callback(m_context, toRef(returnValue.impl()));
    
    m_callback = 0;
    m_disposeCallback = 0;
}

void ScriptReturnValueCallback::invalidate()
{
    ASSERT(m_callback);

    m_disposeCallback(m_context);

    m_callback = 0;
    m_disposeCallback = 0;
}

} // namespace WebKit
