/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "LegacyWebPageDebuggable.h"

#include <WebCore/Document.h>
#include <WebCore/Page.h>
#include <WebCore/PageInspectorController.h>
#include <wtf/TZoneMallocInlines.h>

WTF_MAKE_TZONE_ALLOCATED_IMPL(LegacyWebPageDebuggable);

LegacyWebPageDebuggable::LegacyWebPageDebuggable(LegacyWebPageInspectorController& inspectorController, WebCore::Page& page)
    : m_inspectorController(&inspectorController)
    , m_page(&page)
{
}

Ref<LegacyWebPageDebuggable> LegacyWebPageDebuggable::create(LegacyWebPageInspectorController& controller, WebCore::Page& page)
{
    return adoptRef(*new LegacyWebPageDebuggable(controller, page));
}

String LegacyWebPageDebuggable::name() const
{
    String result;
    callOnMainThreadAndWait([this, protectedThis = Ref { *this }, &result] {
        if (RefPtr page = m_page) {
            if (RefPtr document = page->localTopDocument())
                result = document->title().isolatedCopy();
        }
    });
    return result;
}

String LegacyWebPageDebuggable::url() const
{
    String result;
    callOnMainThreadAndWait([this, protectedThis = Ref { *this }, &result] {
        RefPtr page = m_page;
        if (!page)
            return;

        result = page->mainFrameURL().string().isolatedCopy();
        if (result.isEmpty())
            result = "about:blank"_s;
    });
    return result;
}

bool LegacyWebPageDebuggable::hasLocalDebugger() const
{
    bool result;
    callOnMainThreadAndWait([this, protectedThis = Ref { *this }, &result] {
        RefPtr controller = m_inspectorController;
        result = controller && controller->hasLocalFrontend();
    });
    return result;
}

void LegacyWebPageDebuggable::connect(Inspector::FrontendChannel& frontendChannel, bool isAutomaticConnection, bool immediatelyPause)
{
    UNUSED_PARAM(isAutomaticConnection);
    UNUSED_PARAM(immediatelyPause);

    callOnMainThreadAndWait([this, protectedThis = Ref { *this }, &frontendChannel] {
        if (RefPtr controller = m_inspectorController)
            controller->connectFrontend(frontendChannel);
    });
}

void LegacyWebPageDebuggable::disconnect(Inspector::FrontendChannel& frontendChannel)
{
    callOnMainThreadAndWait([this, protectedThis = Ref { *this }, &frontendChannel] {
        if (RefPtr controller = m_inspectorController)
            controller->disconnectFrontend(frontendChannel);
    });
}

void LegacyWebPageDebuggable::dispatchMessageFromRemote(String&& message)
{
    callOnMainThreadAndWait([this, protectedThis = Ref { *this }, message = WTF::move(message).isolatedCopy()] mutable {
        if (RefPtr controller = m_inspectorController)
            controller->dispatchMessageFromFrontend(WTF::move(message));
    });
}

void LegacyWebPageDebuggable::setIndicating(bool indicating)
{
    callOnMainThreadAndWait([this, protectedThis = Ref { *this }, indicating] {
        if (RefPtr page = m_page)
            protect(page->inspectorController())->setIndicating(indicating);
    });
}

void LegacyWebPageDebuggable::setNameOverride(const String& name)
{
    m_nameOverride = name;
    update();
}

void LegacyWebPageDebuggable::detachFromPage()
{
    m_page = nullptr;
}
