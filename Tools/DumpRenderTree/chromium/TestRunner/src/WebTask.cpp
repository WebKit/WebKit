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
#include "WebTask.h"

#include "WebKit.h"
#include <wtf/Vector.h>

namespace WebTestRunner {

WebTask::WebTask(WebTaskList* list)
    : m_taskList(list)
{
    m_taskList->registerTask(this);
}

WebTask::~WebTask()
{
    if (m_taskList)
        m_taskList->unregisterTask(this);
}

class WebTaskList::Private : public Vector<WebTask*> {
};

WebTaskList::WebTaskList()
    : m_private(new Private)
{
}

WebTaskList::~WebTaskList()
{
    revokeAll();
    delete m_private;
}

void WebTaskList::registerTask(WebTask* task)
{
    m_private->append(task);
}

void WebTaskList::unregisterTask(WebTask* task)
{
    size_t index = m_private->find(task);
    if (index != notFound)
        m_private->remove(index);
}

void WebTaskList::revokeAll()
{
    while (!m_private->isEmpty())
        (*m_private)[0]->cancel();
}

}
