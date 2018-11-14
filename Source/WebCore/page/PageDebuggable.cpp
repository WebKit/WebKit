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
#include "Frame.h"
#include "InspectorController.h"
#include "Page.h"
#include "Settings.h"
#include <JavaScriptCore/InspectorAgentBase.h>

namespace WebCore {

using namespace Inspector;

PageDebuggable::PageDebuggable(Page& page)
    : m_page(page)
{
}

String PageDebuggable::name() const
{
    if (!m_nameOverride.isNull())
        return m_nameOverride;

    if (!m_page.mainFrame().document())
        return String();

    return m_page.mainFrame().document()->title();
}

String PageDebuggable::url() const
{
    if (!m_page.mainFrame().document())
        return String();

    String url = m_page.mainFrame().document()->url().string();
    return url.isEmpty() ? "about:blank"_s : url;
}

bool PageDebuggable::hasLocalDebugger() const
{
    return m_page.inspectorController().hasLocalFrontend();
}

void PageDebuggable::connect(Inspector::FrontendChannel* channel, bool isAutomaticConnection, bool immediatelyPause)
{
    InspectorController& inspectorController = m_page.inspectorController();
    inspectorController.connectFrontend(channel, isAutomaticConnection, immediatelyPause);
}

void PageDebuggable::disconnect(Inspector::FrontendChannel* channel)
{
    InspectorController& inspectorController = m_page.inspectorController();
    inspectorController.disconnectFrontend(channel);
}

void PageDebuggable::dispatchMessageFromRemote(const String& message)
{
    m_page.inspectorController().dispatchMessageFromFrontend(message);
}

void PageDebuggable::setIndicating(bool indicating)
{
    m_page.inspectorController().setIndicating(indicating);
}

void PageDebuggable::setNameOverride(const String& name)
{
    m_nameOverride = name;
    update();
}

} // namespace WebCore

#endif // ENABLE(REMOTE_INSPECTOR)
