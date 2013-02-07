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

#ifndef WebTask_h
#define WebTask_h

namespace WebTestRunner {

class WebTaskList;

// WebTask represents a task which can run by WebTestDelegate::postTask() or
// WebTestDelegate::postDelayedTask().
class WebTask {
public:
    explicit WebTask(WebTaskList*);
    virtual ~WebTask();

    // The main code of this task.
    // An implementation of run() should return immediately if cancel() was called.
    virtual void run() = 0;
    virtual void cancel() = 0;

protected:
    WebTaskList* m_taskList;
};

class WebTaskList {
public:
    WebTaskList();
    ~WebTaskList();
    void registerTask(WebTask*);
    void unregisterTask(WebTask*);
    void revokeAll();

private:
    class Private;
    Private* m_private;
};

// A task containing an object pointer of class T. Derived classes should
// override runIfValid() which in turn can safely invoke methods on the
// m_object. The Class T must have "WebTaskList* taskList()".
template<class T>
class WebMethodTask : public WebTask {
public:
    explicit WebMethodTask(T* object)
        : WebTask(object->taskList())
        , m_object(object)
    {
    }

    virtual ~WebMethodTask() { }

    virtual void run()
    {
        if (m_object)
            runIfValid();
    }

    virtual void cancel()
    {
        m_object = 0;
        m_taskList->unregisterTask(this);
        m_taskList = 0;
    }

    virtual void runIfValid() = 0;

protected:
    T* m_object;
};

}

#endif // WebTask_h
