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
#include "TestController.h"
#include <WebKit2/WKPage.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/text/CString.h>

namespace WTR {

static inline WKPageRef mainPage()
{
    return TestController::shared().mainWebView()->page();
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

WorkQueueManager::WorkQueueManager()
    : m_processing(false)
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
        OwnPtr<WorkQueueItem> item(m_workQueue.takeFirst());
        m_processing = item->invoke();
    }

    return !m_processing;
}

void WorkQueueManager::queueLoad(const String& url, const String& target)
{
    class LoadItem : public WorkQueueItem {
    public:
        LoadItem(const String& url, const String& target)
            : m_url(AdoptWK, WKURLCreateWithUTF8CString(url.utf8().data()))
            , m_target(target)
        {
        }

        bool invoke() const
        {
            if (!m_target.isEmpty()) {
                // FIXME: Use target. Some layout tests cannot pass as they rely on this functionality.
                fprintf(stderr, "queueLoad for a specific target is not implemented.\n");
                return false;
            }
            WKPageLoadURL(mainPage(), m_url.get());
            return true;
        }

        WKRetainPtr<WKURLRef> m_url;
        String m_target;
    };

    enqueue(new LoadItem(url, target));
}

void WorkQueueManager::queueBackNavigation(unsigned howFarBackward)
{
    class BackNavigationItem : public WorkQueueItem {
    public:
        BackNavigationItem(unsigned howFarBackward) : m_howFarBackward(howFarBackward) { }

        bool invoke() const { return goToItemAtIndex(-m_howFarBackward); }

        unsigned m_howFarBackward;
    };

    enqueue(new BackNavigationItem(howFarBackward));
}

void WorkQueueManager::queueReload()
{
    class ReloadItem : public WorkQueueItem {
    public:
        bool invoke() const
        {
            WKPageReload(mainPage());
            return true;
        }
    };

    enqueue(new ReloadItem());
}

void WorkQueueManager::enqueue(WorkQueueItem* item)
{
    ASSERT(item);
    if (m_processing) {
        fprintf(stderr, "Attempt to enqueue a work item while queue is being processed.\n");
        delete item;
        return;
    }

    m_workQueue.append(adoptPtr(item));
}

} // namespace WTR
