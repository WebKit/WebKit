/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WorkQueueItem_h
#define WorkQueueItem_h

#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSBase.h>

class WorkQueueItem {
public:
    virtual ~WorkQueueItem() { }
    virtual void invoke() const = 0;
};

class LoadItem : public WorkQueueItem {
public:
    LoadItem(const JSStringRef url, const JSStringRef target)
        : m_url(url)
        , m_target(target)
    {
    }

    const JSStringRef url() const { return m_url.get(); }
    const JSStringRef target() const { return m_target.get(); }

    virtual void invoke() const;

private:
    JSRetainPtr<JSStringRef> m_url;
    JSRetainPtr<JSStringRef> m_target;
};

class ReloadItem : public WorkQueueItem {
public:
    virtual void invoke() const;
};

class ScriptItem : public WorkQueueItem {
public:
    ScriptItem(const JSStringRef script)
        : m_script(script)
    {
    }

    const JSStringRef script() const { return m_script.get(); }

    virtual void invoke() const;

private:
    JSRetainPtr<JSStringRef> m_script;
};

class BackForwardItem : public WorkQueueItem {
public:
    virtual void invoke() const;

protected:
    BackForwardItem(int howFar)
        : m_howFar(howFar)
    {
    }

    int m_howFar;
};

class BackItem : public BackForwardItem {
public:
    BackItem(unsigned howFar)
        : BackForwardItem(-howFar)
    {
    }
};

class ForwardItem : public BackForwardItem {
public:
    ForwardItem(unsigned howFar)
        : BackForwardItem(howFar)
    {
    }
};

#endif // !defined(WorkQueueItem_h)
