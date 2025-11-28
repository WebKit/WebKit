/*
 * Copyright (C) 2025 Microsoft Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebKitBrowserAutomation.h"

#include "APIAutomationClient.h"
#include "WebKitBrowserAutomationPrivate.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>

/**
 * WebKitBrowserAutomation:
 *
 * WebKitBrowserAutomation represents an automation session of a WebKit application.
 * It is used to implement the WebDriver Bidi protocol (https://w3c.github.io/webdriver-bidi/).
 * It is different from WebKitAutomationSession, which is used to implement the old WebDriver
 * protocol and is bound to a single WebKitWebContext.
 *
 * Since: 2.50
 */

enum {
    CLOSE_BROWSER,

    LAST_SIGNAL
};

struct _WebKitBrowserAutomationPrivate {
    int unused { 0 };
};

static std::array<unsigned, LAST_SIGNAL> signals;

WEBKIT_DEFINE_FINAL_TYPE(WebKitBrowserAutomation, webkit_browser_automation, G_TYPE_OBJECT, GObject)

namespace {

class WebKitAutomationClient final : public API::AutomationClient {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(WebKitAutomationClient);
public:
    explicit WebKitAutomationClient(WebKitBrowserAutomation* webkitBrowserAutomation)
        : m_webkitBrowserAutomation(webkitBrowserAutomation)
    {
    }

private:
    void closeBrowser() override
    {
        g_signal_emit(m_webkitBrowserAutomation.get(), signals[CLOSE_BROWSER], 0);
    }

    GRefPtr<WebKitBrowserAutomation> m_webkitBrowserAutomation;
};

} // namespace

static void webkit_browser_automation_class_init(WebKitBrowserAutomationClass* automationClass)
{
    GObjectClass* gObjectClass = G_OBJECT_CLASS(automationClass);

    /**
     * WebKitBrowserAutomation::close-browser:
     * @browser_automation: the #WebKitBrowserAutomation on which the signal is emitted
     *
     * Emitted when the browser_automation is requested to close the browser application.
     *
     * This signal is emitted when browser_automation receives 'browser.close' WebDriver Bidi command
     * from its remote client. If the signal is not handled the command will fail.
     */
    signals[CLOSE_BROWSER] = g_signal_new(
        "close-browser",
        G_TYPE_FROM_CLASS(gObjectClass),
        G_SIGNAL_RUN_LAST,
        0,
        nullptr, nullptr,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);
}

static WebKitBrowserAutomation* existingBrowserAutomation;

static gpointer createDefaultBrowserAutomation(gpointer)
{
    static GRefPtr<WebKitBrowserAutomation> browserAutomation = adoptGRef(WEBKIT_BROWSER_AUTOMATION(g_object_new(WEBKIT_TYPE_BROWSER_AUTOMATION, nullptr)));
    existingBrowserAutomation = browserAutomation.get();
    return browserAutomation.get();
}

void webkitBrowserAutomationMaybeSetClient(WebKit::WebProcessPool& processPool)
{
    if (!existingBrowserAutomation)
        return;
    processPool.setAutomationClient(makeUnique<WebKitAutomationClient>(existingBrowserAutomation));
}

/**
 * webkit_browser_automation_get_default:
 *
 * Get global #WebKitBrowserAutomation
 *
 * Returns: (transfer none): a #WebKitBrowserAutomation
 *
 * Since: 2.50
 */
WebKitBrowserAutomation* webkit_browser_automation_get_default()
{
    static GOnce onceInit = G_ONCE_INIT;
    return WEBKIT_BROWSER_AUTOMATION(g_once(&onceInit, createDefaultBrowserAutomation, 0));
}
