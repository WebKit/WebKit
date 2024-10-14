/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

#include "config.h"
#include "PageDebuggable.h"

#if ENABLE(REMOTE_INSPECTOR)

#include "Document.h"
#include "InspectorController.h"
#include "LocalFrame.h"
#include "Page.h"
#include "Settings.h"
#include <JavaScriptCore/InspectorAgentBase.h>
#include <wtf/MainThread.h>
#include <wtf/TZoneMallocInlines.h>


namespace WebCore {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(PageDebuggable);
Ref<PageDebuggable> PageDebuggable::create(Page& page)
{
    return adoptRef(*new PageDebuggable(page));
}

PageDebuggable::PageDebuggable(Page& page)
    : m_page(&page)
{
}

String PageDebuggable::name() const
{
    String name;
    callOnMainThreadAndWait([this, protectedThis = Ref { *this }, &name] {
        RefPtr page = m_page.get();
        if (!page)
            return;

        RefPtr localMainFrame = dynamicDowncast<LocalFrame>(page->mainFrame());
        if (!localMainFrame)
            return;

        if (!localMainFrame->document())
            return;

        name = localMainFrame->document()->title().isolatedCopy();
    });
    return name;
}

String PageDebuggable::url() const
{
    String url;
    callOnMainThreadAndWait([this, protectedThis = Ref { *this }, &url] {
        RefPtr page = m_page.get();
        if (!page)
            return;

        RefPtr localMainFrame = dynamicDowncast<LocalFrame>(page->mainFrame());
        if (!localMainFrame)
            return;

        if (!localMainFrame->document())
            return;

        url = localMainFrame->document()->url().string().isolatedCopy();
        if (url.isEmpty())
            url = "about:blank"_s;
    });
    return url;
}

bool PageDebuggable::hasLocalDebugger() const
{
    bool hasLocalDebugger;
    callOnMainThreadAndWait([this, protectedThis = Ref { *this }, &hasLocalDebugger] {
        hasLocalDebugger = m_page && m_page->inspectorController().hasLocalFrontend();
    });
    return hasLocalDebugger;
}

void PageDebuggable::connect(FrontendChannel& channel, bool isAutomaticConnection, bool immediatelyPause)
{
    callOnMainThreadAndWait([this, protectedThis = Ref { *this }, &channel, isAutomaticConnection, immediatelyPause] {
        if (RefPtr page = m_page.get())
            page->inspectorController().connectFrontend(channel, isAutomaticConnection, immediatelyPause);
    });
}

void PageDebuggable::disconnect(FrontendChannel& channel)
{
    callOnMainThreadAndWait([this, protectedThis = Ref { *this }, &channel] {
        if (RefPtr page = m_page.get())
            page->inspectorController().disconnectFrontend(channel);
    });
}

void PageDebuggable::dispatchMessageFromRemote(String&& message)
{
    callOnMainThreadAndWait([this, protectedThis = Ref { *this }, message = WTFMove(message).isolatedCopy()]() mutable {
        if (RefPtr page = m_page.get())
            page->inspectorController().dispatchMessageFromFrontend(WTFMove(message));
    });
}

void PageDebuggable::setIndicating(bool indicating)
{
    callOnMainThreadAndWait([this, protectedThis = Ref { *this }, indicating] {
        if (RefPtr page = m_page.get())
            page->inspectorController().setIndicating(indicating);
    });
}

void PageDebuggable::setNameOverride(const String& name)
{
    m_nameOverride = name;
    update();
}

void PageDebuggable::detachFromPage()
{
    m_page = nullptr;
}

} // namespace WebCore

#endif // ENABLE(REMOTE_INSPECTOR)
