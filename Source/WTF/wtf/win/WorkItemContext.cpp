/*
 * Copyright (C) 2010, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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
#include "WorkItemContext.h"

#include <Windows.h>
#include <wtf/Threading.h>
#include <wtf/WorkQueue.h>

namespace WTF {

WorkItemContext::WorkItemContext(HANDLE handle, HANDLE waitHandle, Function<void()>&& function, WorkQueue* queue)
    : m_handle(handle)
    , m_waitHandle(waitHandle)
    , m_function(WTFMove(function))
    , m_queue(queue)
{
}

Ref<WorkItemContext> WorkItemContext::create(HANDLE handle, HANDLE waitHandle, Function<void()>&& function, WorkQueue* queue)
{
    return adoptRef(*new WorkItemContext(handle, waitHandle, WTFMove(function), queue));
}

WorkItemContext::~WorkItemContext()
{
}

} // namespace WTF
