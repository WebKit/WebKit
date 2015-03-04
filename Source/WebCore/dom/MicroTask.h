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

#ifndef MicroTask_h
#define MicroTask_h

#include <wtf/NeverDestroyed.h>
#include <wtf/Vector.h>


namespace WebCore {

class MicroTask {
public:
    virtual void run() = 0;
    virtual ~MicroTask() { }
};

class MicroTaskQueue {
    friend NeverDestroyed<MicroTaskQueue>;

public:
    WEBCORE_EXPORT static MicroTaskQueue& singleton();
    ~MicroTaskQueue() { }

    WEBCORE_EXPORT void queueMicroTask(std::unique_ptr<MicroTask>);

    void runMicroTasks();

private:
    MicroTaskQueue() { }
    Vector<std::unique_ptr<MicroTask>> m_microTaskQueue;
};

} // namespace WebCore

#endif // MicroTask_h
