/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/FastMalloc.h>
#include <wtf/Noncopyable.h>
#include <wtf/WallTime.h>

namespace WebKit {

class WebPageLoadTiming {
    WTF_MAKE_NONCOPYABLE(WebPageLoadTiming);
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit WebPageLoadTiming(WallTime navigationStart)
        : m_navigationStart(navigationStart)
    { }

    WallTime navigationStart() const { return m_navigationStart; }

    WallTime firstVisualLayout() const { return m_firstVisualLayout; }
    void setFirstVisualLayout(WallTime timestamp) { m_firstVisualLayout = timestamp; }

    WallTime firstMeaningfulPaint() const { return m_firstMeaningfulPaint; }
    void setFirstMeaningfulPaint(WallTime timestamp) { m_firstMeaningfulPaint = timestamp; }

    WallTime documentFinishedLoading() const { return m_documentFinishedLoading; }
    void setDocumentFinishedLoading(WallTime timestamp) { m_documentFinishedLoading = timestamp; }

    WallTime allSubresourcesFinishedLoading() const { return m_allSubresourcesFinishedLoading; }
    void updateEndOfNetworkRequests(WallTime timestamp)
    {
        if (timestamp > m_allSubresourcesFinishedLoading)
            m_allSubresourcesFinishedLoading = timestamp;
    }

private:
    WallTime m_navigationStart;
    WallTime m_firstVisualLayout;
    WallTime m_firstMeaningfulPaint;
    WallTime m_documentFinishedLoading;
    WallTime m_allSubresourcesFinishedLoading;
};

}
