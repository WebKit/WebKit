/*
 * Copyright (C) 2014 Yoav Weiss (yoav@yoav.ws)
 * Copyright (C) 2015 Akamai Technologies Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "MicroTask.h"

#include "Document.h"

namespace WebCore {

MicroTaskQueue& MicroTaskQueue::singleton()
{
    ASSERT(isMainThread());
    static NeverDestroyed<MicroTaskQueue> queue;
    return queue;
}

void MicroTaskQueue::queueMicroTask(std::unique_ptr<MicroTask> task)
{
    ASSERT(isMainThread());
    m_microTaskQueue.append(WTF::move(task));
}

void MicroTaskQueue::runMicroTasks()
{
    ASSERT(isMainThread());
    for (auto& task : m_microTaskQueue)
        task->run();
    m_microTaskQueue.clear();
}

} // namespace WebCore

