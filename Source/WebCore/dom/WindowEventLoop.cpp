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

#include "config.h"
#include "WindowEventLoop.h"

#include "Document.h"

namespace WebCore {

Ref<WindowEventLoop> WindowEventLoop::create()
{
    return adoptRef(*new WindowEventLoop);
}

WindowEventLoop::WindowEventLoop()
{
}

void WindowEventLoop::queueTask(TaskSource source, Document& document, TaskFunction&& task)
{
    if (m_tasks.isEmpty()) {
        callOnMainThread([eventLoop = makeRef(*this)] () {
            eventLoop->run();
        });
    }
    m_tasks.append(Task { source, WTFMove(task), document.identifier() });
}

void WindowEventLoop::run()
{
    Vector<Task> tasks = WTFMove(m_tasks);
    ASSERT(m_tasks.isEmpty());
    for (auto& task : tasks) {
        auto* document = Document::allDocumentsMap().get(task.documentIdentifier);
        if (!document || document->activeDOMObjectsAreStopped())
            continue;
        // Skip tasks associated with suspended documents.
        task.task();
    }
}

} // namespace WebCore
