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

#include "config.h"
#include "WebPageTesting.h"

#include "NotificationPermissionRequestManager.h"
#include "PluginView.h"
#include "WebNotificationClient.h"
#include "WebPage.h"
#include "WebPageTestingMessages.h"
#include "WebProcess.h"
#include <WebCore/Editor.h>
#include <WebCore/FocusController.h>
#include <WebCore/IntPoint.h>
#include <WebCore/NotificationController.h>
#include <WebCore/Page.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebPageTesting);

WebPageTesting::WebPageTesting(WebPage& page)
    : m_page(page)
{
    WebProcess::singleton().addMessageReceiver(Messages::WebPageTesting::messageReceiverName(), m_page->identifier(), *this);
}

WebPageTesting::~WebPageTesting()
{
    WebProcess::singleton().removeMessageReceiver(Messages::WebPageTesting::messageReceiverName(), m_page->identifier());
}

void WebPageTesting::setDefersLoading(bool defersLoading)
{
    if (RefPtr page = m_page->corePage())
        page->setDefersLoading(defersLoading);
}

void WebPageTesting::isLayerTreeFrozen(CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(!!m_page->layerTreeFreezeReasons());
}

void WebPageTesting::setPermissionLevel(const String& origin, bool allowed)
{
#if ENABLE(NOTIFICATIONS)
    if (RefPtr notificationPermissionRequestManager = protectedPage()->notificationPermissionRequestManager())
        notificationPermissionRequestManager->setPermissionLevelForTesting(origin, allowed);
#else
    UNUSED_PARAM(origin);
    UNUSED_PARAM(allowed);
#endif
}

void WebPageTesting::isEditingCommandEnabled(const String& commandName, CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr page = m_page->corePage();
    RefPtr frame = page->checkedFocusController()->focusedOrMainFrame();
    if (!frame)
        return completionHandler(false);

#if ENABLE(PDF_PLUGIN)
    if (RefPtr pluginView = protectedPage()->focusedPluginViewForFrame(*frame))
        return completionHandler(pluginView->isEditingCommandEnabled(commandName));
#endif

    auto command = frame->checkedEditor()->command(commandName);
    completionHandler(command.isSupported() && command.isEnabled());
}

#if ENABLE(NOTIFICATIONS)
void WebPageTesting::clearNotificationPermissionState()
{
    RefPtr page = m_page->corePage();
    auto& client = static_cast<WebNotificationClient&>(WebCore::NotificationController::from(page.get())->client());
    client.clearNotificationPermissionState();
}
#endif

void WebPageTesting::clearWheelEventTestMonitor()
{
    RefPtr page = m_page->corePage();
    if (!page)
        return;

    page->clearWheelEventTestMonitor();
}

void WebPageTesting::setTopContentInset(float contentInset, CompletionHandler<void()>&& completionHandler)
{
    protectedPage()->setTopContentInset(contentInset);
    completionHandler();
}

void WebPageTesting::setPageScaleFactor(double scale, IntPoint origin, CompletionHandler<void()>&& completionHandler)
{
    RefPtr page = m_page->corePage();
    if (!page)
        return completionHandler();
    page->setPageScaleFactor(scale, origin);
    completionHandler();
}

Ref<WebPage> WebPageTesting::protectedPage() const
{
    return m_page.get();
}

} // namespace WebKit
