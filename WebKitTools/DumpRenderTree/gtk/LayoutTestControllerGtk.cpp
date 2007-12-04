/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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

#include "LayoutTestController.h"

#include "DumpRenderTree.h"
#include "WorkQueue.h"
#include "WorkQueueItem.h"
#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSStringRef.h>

LayoutTestController::~LayoutTestController()
{
    // FIXME: implement
}

void LayoutTestController::addDisallowedURL(JSStringRef url)
{
    // FIXME: implement
}

void LayoutTestController::clearBackForwardList()
{
    // FIXME: implement
}

JSStringRef LayoutTestController::copyDecodedHostName(JSStringRef name)
{
    // FIXME: implement
    return 0;
}

JSStringRef LayoutTestController::copyEncodedHostName(JSStringRef name)
{
    // FIXME: implement
    return 0;
}

void LayoutTestController::display()
{
    displayWebView();
}

void LayoutTestController::keepWebHistory()
{
    // FIXME: implement
}

void LayoutTestController::notifyDone()
{
    // FIXME: The frame might not be finished loading yet
    if (m_waitToDump && !WorkQueue::shared()->count())
        dump();
    m_waitToDump = false;
}

void LayoutTestController::queueBackNavigation(int howFarBack)
{
    WorkQueue::shared()->queue(new BackItem(howFarBack));
}

void LayoutTestController::queueForwardNavigation(int howFarForward)
{
    WorkQueue::shared()->queue(new ForwardItem(howFarForward));
}

void LayoutTestController::queueLoad(JSStringRef url, JSStringRef target)
{
    // FIXME: We need to resolve relative URLs here
    WorkQueue::shared()->queue(new LoadItem(url, target));
}

void LayoutTestController::queueReload()
{
    WorkQueue::shared()->queue(new ReloadItem);
}

void LayoutTestController::queueScript(JSStringRef script)
{
    WorkQueue::shared()->queue(new ScriptItem(script));
}

void LayoutTestController::setAcceptsEditing(bool newAcceptsEditing)
{
    // FIXME: implement
}

void LayoutTestController::setCustomPolicyDelegate(bool setDelegate)
{
    // FIXME: implement
}

void LayoutTestController::setMainFrameIsFirstResponder(bool flag)
{
    // FIXME: implement
}

void LayoutTestController::setTabKeyCyclesThroughElements(bool cycles)
{
    // FIXME: implement
}

void LayoutTestController::setUseDashboardCompatibilityMode(bool flag)
{
    // FIXME: implement
}

void LayoutTestController::setUserStyleSheetEnabled(bool flag)
{
    // FIXME: implement
}

void LayoutTestController::setUserStyleSheetLocation(JSStringRef path)
{
    // FIXME: implement
}

void LayoutTestController::setWindowIsKey(bool windowIsKey)
{
    // FIXME: implement
}

void LayoutTestController::setWaitToDump(bool waitUntilDone)
{
    m_waitToDump = waitUntilDone;
    // FIXME: Should have some sort of watchdog timer here
}

int LayoutTestController::windowCount()
{
    // FIXME: implement
    return 0;
}

void LayoutTestController::setPrivateBrowsingEnabled(bool privateBrowsingEnabled)
{
    // FIXME: implement
}

void LayoutTestController::setAuthorAndUserStylesEnabled(bool flag)
{
    // FIXME: implement
}

