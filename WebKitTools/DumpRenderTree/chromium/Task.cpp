/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Task.h"

#include "WebKit.h"
#include "WebKitClient.h"
#include "webkit/support/webkit_support.h"

WebTask::WebTask(TaskList* list): m_taskList(list) { m_taskList->registerTask(this); }
WebTask::~WebTask() { m_taskList->unregisterTask(this); }

void TaskList::unregisterTask(WebTask* task)
{
    size_t index = m_tasks.find(task);
    if (index != notFound)
        m_tasks.remove(index);
}

void TaskList::revokeAll()
{
    for (unsigned i = 0; i < m_tasks.size(); ++i)
        m_tasks[i]->cancel();
    m_tasks.clear();
}

static void invokeTask(void* context)
{
    WebTask* task = static_cast<WebTask*>(context);
    task->run();
    delete task;
}

void postTask(WebTask* task)
{
    WebKit::webKitClient()->callOnMainThread(invokeTask, static_cast<void*>(task));
}

void postDelayedTask(WebTask* task, int64_t ms)
{
    webkit_support::PostDelayedTask(invokeTask, static_cast<void*>(task), ms);
}


