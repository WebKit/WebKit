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

#include "URLSchemeTaskParameters.h"
#include "WebPageProxy.h"
#include "WebURLSchemeTask.h"

namespace WebKit {
using namespace WebCore;

WebURLSchemeHandler::WebURLSchemeHandler()
    : m_identifier(WebURLSchemeHandlerIdentifier::generate())
{
}

WebURLSchemeHandler::~WebURLSchemeHandler()
{
    ASSERT(m_tasks.isEmpty());
}

void WebURLSchemeHandler::startTask(WebPageProxy& page, WebProcessProxy& process, PageIdentifier webPageID, URLSchemeTaskParameters&& parameters, SyncLoadCompletionHandler&& completionHandler)
{
    auto taskIdentifier = parameters.taskIdentifier;
    auto result = m_tasks.add({ taskIdentifier, page.identifier() }, WebURLSchemeTask::create(*this, page, process, webPageID, WTFMove(parameters), WTFMove(completionHandler)));
    ASSERT(result.isNewEntry);

    auto pageEntry = m_tasksByPageIdentifier.add(page.identifier(), HashSet<WebCore::ResourceLoaderIdentifier>());
    ASSERT(!pageEntry.iterator->value.contains(taskIdentifier));
    pageEntry.iterator->value.add(taskIdentifier);

    platformStartTask(page, result.iterator->value);
}

WebProcessProxy* WebURLSchemeHandler::processForTaskIdentifier(WebPageProxy& page, WebCore::ResourceLoaderIdentifier taskIdentifier) const
{
    auto key = std::make_pair(taskIdentifier, page.identifier());
    if (!decltype(m_tasks)::isValidKey(key))
        return nullptr;
    auto iterator = m_tasks.find(key);
    if (iterator == m_tasks.end())
        return nullptr;
    return iterator->value->process();
}

void WebURLSchemeHandler::stopAllTasksForPage(WebPageProxy& page, WebProcessProxy* process)
{
    auto iterator = m_tasksByPageIdentifier.find(page.identifier());
    if (iterator == m_tasksByPageIdentifier.end())
        return;

    auto& tasksByPage = iterator->value;
    Vector<WebCore::ResourceLoaderIdentifier> taskIdentifiersToStop;
    taskIdentifiersToStop.reserveInitialCapacity(tasksByPage.size());
    for (auto taskIdentifier : tasksByPage) {
        if (!process || processForTaskIdentifier(page, taskIdentifier) == process)
            taskIdentifiersToStop.uncheckedAppend(taskIdentifier);
    }

    for (auto& taskIdentifier : taskIdentifiersToStop)
        stopTask(page, taskIdentifier);

}

void WebURLSchemeHandler::stopTask(WebPageProxy& page, WebCore::ResourceLoaderIdentifier taskIdentifier)
{
    auto key = std::make_pair(taskIdentifier, page.identifier());
    if (!decltype(m_tasks)::isValidKey(key))
        return;
    auto iterator = m_tasks.find(key);
    if (iterator == m_tasks.end())
        return;

    iterator->value->stop();
    platformStopTask(page, iterator->value);

    removeTaskFromPageMap(page.identifier(), taskIdentifier);
    m_tasks.remove(iterator);
}

void WebURLSchemeHandler::taskCompleted(WebPageProxyIdentifier pageID, WebURLSchemeTask& task)
{
    auto takenTask = m_tasks.take({ task.resourceLoaderID(), pageID });
    ASSERT_UNUSED(takenTask, takenTask == &task);
    removeTaskFromPageMap(task.pageProxyID(), task.resourceLoaderID());

    platformTaskCompleted(task);
}

void WebURLSchemeHandler::removeTaskFromPageMap(WebPageProxyIdentifier pageID, WebCore::ResourceLoaderIdentifier taskID)
{
    auto iterator = m_tasksByPageIdentifier.find(pageID);
    ASSERT(iterator != m_tasksByPageIdentifier.end());
    ASSERT(iterator->value.contains(taskID));
    if (!decltype(iterator->value)::isValidValue(taskID))
        return;
    iterator->value.remove(taskID);
    if (iterator->value.isEmpty())
        m_tasksByPageIdentifier.remove(iterator);
}

} // namespace WebKit
