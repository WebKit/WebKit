/*
* Copyright (C) 2014-2015 Apple Inc.  All rights reserved.
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
* THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
* PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
* OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "stdafx.h"
#include "PageLoadTestClient.h"

#include "WebKitLegacyBrowserWindow.h"
#include <WebCore/PlatformExportMacros.h>
#include <cmath>
#include <wtf/Assertions.h>
#include <wtf/FilePrintStream.h>
#include <wtf/ProcessID.h>

static const CFTimeInterval waitForNewResourceLoadDuration = 0.1;

PageLoadTestClient::PageLoadTestClient(WebKitLegacyBrowserWindow* host, bool pageLoadTesting)
    : m_host(host)
    , m_waitForLoadToReallyEnd(this, &PageLoadTestClient::endPageLoad)
    , m_repetitions(pageLoadTesting ? 20 : 1)
    , m_pageLoadTesting(pageLoadTesting)
{
}

void PageLoadTestClient::pageLoadStartedAtTime(CFAbsoluteTime startTime)
{
    m_startTimes.append(startTime);
}

void PageLoadTestClient::didStartProvisionalLoad(IWebFrame& frame)
{
    _com_ptr_t<_com_IIID<IWebFrame2, &__uuidof(IWebFrame2)>> frame2;
    if (FAILED(frame.QueryInterface(&frame2.GetInterfacePtr())))
        return;

    BOOL mainFrame = FALSE;
    if (frame2 && FAILED(frame2->isMainFrame(&mainFrame)))
        return;

    if (mainFrame) {
        clearPageLoadState();
        pageLoadStartedAtTime(CFAbsoluteTimeGetCurrent());
    }
}

void PageLoadTestClient::didCommitLoad()
{
    ++m_frames;
}

void PageLoadTestClient::didFirstLayoutForMainFrame()
{
    // Nothing to do
}

void PageLoadTestClient::didHandleOnLoadEvents()
{
    ++m_onLoadEvents;
    maybeEndPageLoadSoon();
}

void PageLoadTestClient::didFinishLoad()
{
    m_currentPageLoadFinished = true;
    maybeEndPageLoadSoon();
}

void PageLoadTestClient::didFailLoad()
{
    --m_frames;
}

void PageLoadTestClient::didInitiateResourceLoad(uint64_t resourceIdentifier)
{
    if (!resourceIdentifier) {
        WTFLogAlways("Saw resourceIdentifier=0. This must be a bug in the loader code. The results may be invalid.");
        return;
    }
    m_loadingSubresources.add(resourceIdentifier);
}

void PageLoadTestClient::didEndResourceLoad(uint64_t resourceIdentifier)
{
    if (!resourceIdentifier) {
        WTFLogAlways("Saw resourceIdentifier=0. This must be a bug in the loader code. The results may be invalid.");
        return;
    }
    auto found = m_loadingSubresources.find(resourceIdentifier);
    if (found == m_loadingSubresources.end())
        return;
    m_loadingSubresources.remove(found);
    maybeEndPageLoadSoon();
}

void PageLoadTestClient::pageLoadEndedAtTime(CFAbsoluteTime endTime)
{
    ASSERT(!m_pageLoadTesting || m_endTimes.size() == m_currentRepetition);
    m_endTimes.append(endTime);

    if (m_currentRepetition) {
        CFTimeInterval pageLoadTime = (m_endTimes[m_currentRepetition] - m_startTimes[m_currentRepetition]);
        if (pageLoadTime > m_longestTime)
            m_longestTime = pageLoadTime;

        m_totalTime += pageLoadTime;
        m_totalSquareRootsOfTime += std::sqrt(pageLoadTime);
        m_pagesTimed++;
        m_geometricMeanProductSum *= pageLoadTime;
    }

    if (m_pageLoadTesting) {
        ++m_currentRepetition;
        if (m_currentRepetition != m_repetitions)
            m_host->loadURL(m_url);
        else {
            dumpRunStatistics();
            m_host->exitProgram();
        }
    }
}

void PageLoadTestClient::endPageLoad(Timer<PageLoadTestClient>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_waitForLoadToReallyEnd);

    if (!shouldConsiderPageLoadEnded())
        return;
    pageLoadEndedAtTime(m_pageLoadEndTime);
    clearPageLoadState();
}

void PageLoadTestClient::clearPageLoadState()
{
    m_currentPageLoadFinished = false;
    m_frames = 0;
    m_onLoadEvents = 0;
    m_pageLoadEndTime = 0;
    m_loadingSubresources.clear();
    m_waitForLoadToReallyEnd.invalidate();
}

bool PageLoadTestClient::shouldConsiderPageLoadEnded() const
{
    return m_currentPageLoadFinished && m_onLoadEvents == m_frames && m_loadingSubresources.isEmpty();
}

void PageLoadTestClient::maybeEndPageLoadSoon()
{
    if (!shouldConsiderPageLoadEnded())
        return;
    m_pageLoadEndTime = CFAbsoluteTimeGetCurrent();
    if (m_waitForLoadToReallyEnd.isValid())
        m_waitForLoadToReallyEnd.invalidate();
    m_waitForLoadToReallyEnd.schedule(waitForNewResourceLoadDuration, false);
}

#if OS(WINDOWS)
void PageLoadTestClient::setPageURL(const _bstr_t& pageURL)
{
    m_url = pageURL;
}
#endif

void PageLoadTestClient::dumpRunStatistics()
{
    const long maxPathLength = 1024;

    char filenameSuffix[maxPathLength + 1];

    snprintf(filenameSuffix, sizeof(filenameSuffix), ".%d.txt", getCurrentProcessID());

    const char* filename = "webkit_performance_log";
    char actualFilename[maxPathLength + 1];

    snprintf(actualFilename, sizeof(actualFilename), "%s%s", filename, filenameSuffix);

    std::unique_ptr<WTF::FilePrintStream> file = WTF::FilePrintStream::open(actualFilename, "w");
    if (!file) {
        WTFLogAlways("Warning: Could not open page load performance data file %s for writing.\n", actualFilename);
        return;
    }

    WTFLogAlways("*** Page load performance data output to \"%s\" ***\n", actualFilename);

    file->print("INDIVIDUAL URL LOAD TIMES:\n");

    CFTimeInterval pageLoadTime = m_endTimes.last() - m_startTimes.last();
    file->printf("Load Time = %.1f ms\t%s\n", pageLoadTime * 1000.0, static_cast<const char*>(m_url));

    double meanTime = 0;
    double squareMeanRootTime = 0;
    double geometricMeanTime = 0;
    if (m_pagesTimed) {
        meanTime = m_totalTime / m_pagesTimed;
        squareMeanRootTime = (m_totalSquareRootsOfTime / m_pagesTimed) * (m_totalSquareRootsOfTime / m_pagesTimed);
        geometricMeanTime = pow(m_geometricMeanProductSum, (1.0 / m_pagesTimed));
    }

    file->printf("FINISHED:    Total Time = %.1f ms\n", m_totalTime * 1000.0);
    file->printf("           Longest Time = %.1f ms\n", m_longestTime * 1000.0);
    file->printf("              Mean Time = %.1f ms\n", meanTime * 1000.0);
    file->printf("  Square-Mean-Root Time = %.1f ms\n", squareMeanRootTime * 1000.0);
    file->printf("    Geometric Mean Time = %.1f ms\n", geometricMeanTime * 1000.0);
    file->printf("---------------------------------------------------------------------------------------------------\n");

    file->flush();
}
