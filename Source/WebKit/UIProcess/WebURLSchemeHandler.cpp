/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "WebURLSchemeHandler.h"

#include "WebPageProxy.h"
#include "WebURLSchemeTask.h"

namespace WebKit {
using namespace WebCore;

static uint64_t generateWebURLSchemeHandlerIdentifier()
{
    static uint64_t nextIdentifier = 1;
    return nextIdentifier++;
}

WebURLSchemeHandler::WebURLSchemeHandler()
    : m_identifier(generateWebURLSchemeHandlerIdentifier())
{
}

WebURLSchemeHandler::~WebURLSchemeHandler()
{
    ASSERT(m_tasks.isEmpty());
}

void WebURLSchemeHandler::startTask(WebPageProxy& page, WebProcessProxy& process, uint64_t taskIdentifier, ResourceRequest&& request, SyncLoadCompletionHandler&& completionHandler)
{
    auto result = m_tasks.add(taskIdentifier, WebURLSchemeTask::create(*this, page, process, taskIdentifier, WTFMove(request), WTFMove(completionHandler)));
    ASSERT(result.isNewEntry);

    auto pageEntry = m_tasksByPageIdentifier.add(page.pageID(), HashSet<uint64_t>());
    ASSERT(!pageEntry.iterator->value.contains(taskIdentifier));
    pageEntry.iterator->value.add(taskIdentifier);

    platformStartTask(page, result.iterator->value);
}

WebProcessProxy* WebURLSchemeHandler::processForTaskIdentifier(uint64_t taskIdentifier) const
{
    auto iterator = m_tasks.find(taskIdentifier);
    if (iterator == m_tasks.end())
        return nullptr;
    return iterator->value->process();
}

void WebURLSchemeHandler::stopAllTasksForPage(WebPageProxy& page, WebProcessProxy* process)
{
    auto iterator = m_tasksByPageIdentifier.find(page.pageID());
    if (iterator == m_tasksByPageIdentifier.end())
        return;

    auto& tasksByPage = iterator->value;
    Vector<uint64_t> taskIdentifiersToStop;
    taskIdentifiersToStop.reserveInitialCapacity(tasksByPage.size());
    for (auto taskIdentifier : tasksByPage) {
        if (!process || processForTaskIdentifier(taskIdentifier) == process)
            taskIdentifiersToStop.uncheckedAppend(taskIdentifier);
    }

    for (auto& taskIdentifier : taskIdentifiersToStop)
        stopTask(page, taskIdentifier);

}

void WebURLSchemeHandler::stopTask(WebPageProxy& page, uint64_t taskIdentifier)
{
    auto iterator = m_tasks.find(taskIdentifier);
    if (iterator == m_tasks.end())
        return;

    iterator->value->stop();
    platformStopTask(page, iterator->value);

    removeTaskFromPageMap(page.pageID(), taskIdentifier);
    m_tasks.remove(iterator);
}

void WebURLSchemeHandler::taskCompleted(WebURLSchemeTask& task)
{
    auto takenTask = m_tasks.take(task.identifier());
    ASSERT_UNUSED(takenTask, takenTask->ptr() == &task);
    removeTaskFromPageMap(task.pageID(), task.identifier());

    platformTaskCompleted(task);
}

void WebURLSchemeHandler::removeTaskFromPageMap(PageIdentifier pageID, uint64_t taskID)
{
    auto iterator = m_tasksByPageIdentifier.find(pageID);
    ASSERT(iterator != m_tasksByPageIdentifier.end());
    ASSERT(iterator->value.contains(taskID));
    iterator->value.remove(taskID);
    if (iterator->value.isEmpty())
        m_tasksByPageIdentifier.remove(iterator);
}

} // namespace WebKit
