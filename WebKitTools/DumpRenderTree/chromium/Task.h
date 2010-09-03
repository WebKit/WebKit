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

#ifndef Task_h
#define Task_h

#include <wtf/Vector.h>

class TaskList;

// WebTask represents a task which can run by postTask() or postDelayedTask().
// it is named "WebTask", not "Task", to avoid conflist with base/task.h.
class WebTask {
public:
    WebTask(TaskList*);
    // The main code of this task.
    // An implementation of run() should return immediately if cancel() was called.
    virtual void run() = 0;
    virtual void cancel() = 0;
    virtual ~WebTask();
private:
    TaskList* m_taskList;
};

class TaskList {
public:
    TaskList() {}
    ~TaskList() { revokeAll(); }
    void registerTask(WebTask* task) { m_tasks.append(task); }
    void unregisterTask(WebTask* task);
    void revokeAll();
private:
    Vector<WebTask*> m_tasks;
};

// A task containing an object pointer of class T. Is is supposed that
// runifValid() calls a member function of the object pointer.
// Class T must have "TaskList* taskList()".
template<class T> class MethodTask: public WebTask {
public:
    MethodTask(T* object): WebTask(object->taskList()), m_object(object) {}
    virtual void run()
    {
        if (m_object)
            runIfValid();
    }
    virtual void cancel() { m_object = 0; }
    virtual void runIfValid() = 0;
protected:
    T* m_object;
};

void postTask(WebTask* task);
void postDelayedTask(WebTask* task, int64_t ms);

#endif // Task_h
