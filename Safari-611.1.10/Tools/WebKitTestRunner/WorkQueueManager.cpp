/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
#include "WorkQueueManager.h"

#include "PlatformWebView.h"
#include "StringFunctions.h"
#include "TestController.h"
#include <WebKit/WKPage.h>
#include <WebKit/WKPagePrivate.h>
#include <WebKit/WKRetainPtr.h>
#include <stdio.h>
#include <wtf/text/CString.h>

namespace WTR {

static inline WKPageRef mainPage()
{
    return TestController::singleton().mainWebView()->page();
}

static inline bool goToItemAtIndex(int index)
{
    WKBackForwardListRef backForwardList = WKPageGetBackForwardList(mainPage());
    ASSERT(backForwardList);

    WKBackForwardListItemRef listItem = WKBackForwardListGetItemAtIndex(backForwardList, index);
    if (!listItem)
        return false;

    WKPageGoToBackForwardListItem(mainPage(), listItem);

    return true;
}

class WorkQueueItem {
public:
    enum Type {
        Loading,
        NonLoading
    };

    virtual ~WorkQueueItem() { }
    virtual Type invoke() const = 0;
};

// Required by WKPageRunJavaScriptInMainFrame().
static void runJavaScriptFunction(WKSerializedScriptValueRef, WKErrorRef, void*)
{
}

template <WorkQueueItem::Type type>
class ScriptItem : public WorkQueueItem {
public:
    explicit ScriptItem(const String& script)
        : m_script(toWK(script))
    {
    }

    WorkQueueItem::Type invoke() const
    {
        WKPageRunJavaScriptInMainFrame(mainPage(), m_script.get(), 0, runJavaScriptFunction);
        return type;
    }

    WKRetainPtr<WKStringRef> m_script;
};

class NavigationItem : public WorkQueueItem {
public:
    explicit NavigationItem(int index) : m_index(index) { }

    WorkQueueItem::Type invoke() const
    {
        return goToItemAtIndex(m_index) ? WorkQueueItem::Loading : WorkQueueItem::NonLoading;
    }

    unsigned m_index;
};

WorkQueueManager::WorkQueueManager()
    : m_processing(false)
{
}

WorkQueueManager::~WorkQueueManager()
{
}

void WorkQueueManager::clearWorkQueue()
{
    m_processing = false;
    m_workQueue.clear();
}

bool WorkQueueManager::processWorkQueue()
{
    m_processing = false;
    while (!m_processing && !m_workQueue.isEmpty()) {
        std::unique_ptr<WorkQueueItem> item(m_workQueue.takeFirst());
        m_processing = (item->invoke() == WorkQueueItem::Loading);
    }

    return !m_processing;
}

void WorkQueueManager::queueLoad(const String& url, const String& target, bool shouldOpenExternalURLs)
{
    class LoadItem : public WorkQueueItem {
    public:
        LoadItem(const String& url, const String& target, bool shouldOpenExternalURLs)
            : m_url(adoptWK(WKURLCreateWithUTF8CString(url.utf8().data())))
            , m_target(target)
            , m_shouldOpenExternalURLs(shouldOpenExternalURLs)
        {
        }

        WorkQueueItem::Type invoke() const
        {
            if (!m_target.isEmpty()) {
                // FIXME: Use target. Some layout tests cannot pass as they rely on this functionality.
                fprintf(stderr, "queueLoad for a specific target is not implemented.\n");
                return WorkQueueItem::NonLoading;
            }
            WKPageLoadURLWithShouldOpenExternalURLsPolicy(mainPage(), m_url.get(), m_shouldOpenExternalURLs);
            return WorkQueueItem::Loading;
        }

        WKRetainPtr<WKURLRef> m_url;
        String m_target;
        bool m_shouldOpenExternalURLs;
    };

    enqueue(new LoadItem(url, target, shouldOpenExternalURLs));
}

void WorkQueueManager::queueLoadHTMLString(const String& content, const String& baseURL, const String& unreachableURL)
{
    class LoadHTMLStringItem : public WorkQueueItem {
    public:
        LoadHTMLStringItem(const String& content, const String& baseURL, const String& unreachableURL)
            : m_content(toWK(content))
            , m_baseURL(adoptWK(WKURLCreateWithUTF8CString(baseURL.utf8().data())))
            , m_unreachableURL(adoptWK(WKURLCreateWithUTF8CString(unreachableURL.utf8().data())))
        {
        }

        WorkQueueItem::Type invoke() const
        {
            WKPageLoadAlternateHTMLString(mainPage(), m_content.get(), m_baseURL.get(), m_unreachableURL.get());
            return WorkQueueItem::Loading;
        }

        WKRetainPtr<WKStringRef> m_content;
        WKRetainPtr<WKURLRef> m_baseURL;
        WKRetainPtr<WKURLRef> m_unreachableURL;
    };

    enqueue(new LoadHTMLStringItem(content, baseURL, unreachableURL));
}

void WorkQueueManager::queueBackNavigation(unsigned howFarBackward)
{
    enqueue(new NavigationItem(-howFarBackward));
}

void WorkQueueManager::queueForwardNavigation(unsigned howFarForward)
{
    enqueue(new NavigationItem(howFarForward));
}

void WorkQueueManager::queueReload()
{
    class ReloadItem : public WorkQueueItem {
    public:
        WorkQueueItem::Type invoke() const
        {
            WKPageReload(mainPage());
            return WorkQueueItem::Loading;
        }
    };

    enqueue(new ReloadItem());
}

void WorkQueueManager::queueLoadingScript(const String& script)
{
    enqueue(new ScriptItem<WorkQueueItem::Loading>(script));
}

void WorkQueueManager::queueNonLoadingScript(const String& script)
{
    enqueue(new ScriptItem<WorkQueueItem::NonLoading>(script));
}

void WorkQueueManager::enqueue(WorkQueueItem* item)
{
    ASSERT(item);
    if (m_processing) {
        fprintf(stderr, "Attempt to enqueue a work item while queue is being processed.\n");
        delete item;
        return;
    }

    m_workQueue.append(std::unique_ptr<WorkQueueItem>(item));
}

} // namespace WTR
