/*
 * Copyright (C) 2011 ProFUSION Embedded Systems
 * Copyright (C) 2011 Samsung Electronics
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

#ifndef WorkQueueItemEfl_h
#define WorkQueueItemEfl_h

#include <JavaScriptCore/wtf/text/WTFString.h>

class WorkQueueItem {
public:
    virtual ~WorkQueueItem() { }
    virtual bool invoke() const = 0; // Returns true if this started a load.
};

class LoadItem : public WorkQueueItem {
public:
    LoadItem(const WTF::String& url, const WTF::String& target)
        : m_target(target)
        , m_url(url)
    {
    }

    virtual bool invoke() const;

private:
    WTF::String m_target;
    WTF::String m_url;
};

class LoadHTMLStringItem : public WorkQueueItem {
public:
    LoadHTMLStringItem(const WTF::String& content, const WTF::String& baseURL, const WTF::String& unreachableURL = WTF::String())
        : m_content(content)
        , m_baseURL(baseURL)
        , m_unreachableURL(unreachableURL)
    {
    }

private:
    virtual bool invoke() const;

    WTF::String m_content;
    WTF::String m_baseURL;
    WTF::String m_unreachableURL;
};

class ReloadItem : public WorkQueueItem {
private:
    virtual bool invoke() const;
};

class ScriptItem : public WorkQueueItem {
protected:
    ScriptItem(const WTF::String& script)
        : m_script(script)
    {
    }

protected:
    virtual bool invoke() const;

private:
    WTF::String m_script;
};

class LoadingScriptItem : public ScriptItem {
public:
    LoadingScriptItem(const WTF::String& script)
        : ScriptItem(script)
    {
    }

private:
    virtual bool invoke() const { return ScriptItem::invoke(); }
};

class NonLoadingScriptItem : public ScriptItem {
public:
    NonLoadingScriptItem(const WTF::String& script)
        : ScriptItem(script)
    {
    }

private:
    virtual bool invoke() const { ScriptItem::invoke(); return false; }
};

class BackForwardItem : public WorkQueueItem {
protected:
    BackForwardItem(int howFar)
        : m_howFar(howFar)
    {
    }

private:
    virtual bool invoke() const;

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

#endif // WorkQueueEfl_h
