/*
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 * Copyright (C) 2011, 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "InspectorClientBlackBerry.h"

#include "BackingStore.h"
#include "Frame.h"
#include "NotImplemented.h"
#include "RenderObject.h"
#include "WebPageClient.h"
#include "WebPage_p.h"

namespace WebCore {

InspectorClientBlackBerry::InspectorClientBlackBerry(BlackBerry::WebKit::WebPagePrivate* webPagePrivate)
    : m_webPagePrivate(webPagePrivate)
{
    m_inspectorSettingsMap = adoptPtr(new SettingsMap);
}

void InspectorClientBlackBerry::inspectorDestroyed()
{
    delete this;
}

Page* InspectorClientBlackBerry::createPage()
{
    notImplemented();
    return 0;
}

String InspectorClientBlackBerry::localizedStringsURL()
{
    notImplemented();
    return String();
}

String InspectorClientBlackBerry::hiddenPanels()
{
    notImplemented();
    return String();
}

void InspectorClientBlackBerry::showWindow()
{
    notImplemented();
}

void InspectorClientBlackBerry::closeWindow()
{
    notImplemented();
}

void InspectorClientBlackBerry::attachWindow()
{
    notImplemented();
}

void InspectorClientBlackBerry::detachWindow()
{
    notImplemented();
}

void InspectorClientBlackBerry::setAttachedWindowHeight(unsigned)
{
    notImplemented();
}

void InspectorClientBlackBerry::highlight()
{
    hideHighlight();
}

void InspectorClientBlackBerry::hideHighlight()
{
    if (!m_webPagePrivate->mainFrame() || !m_webPagePrivate->mainFrame()->document() || !m_webPagePrivate->mainFrame()->document()->documentElement()
        || !m_webPagePrivate->mainFrame()->document()->documentElement()->renderer())
        return;

    // FIXME: Potentially slow hack, but invalidating everything should work since the actual highlight is drawn by BackingStorePrivate::renderContents().
    m_webPagePrivate->mainFrame()->document()->documentElement()->renderer()->repaint(true);
}

void InspectorClientBlackBerry::inspectedURLChanged(const String&)
{
    notImplemented();
}

void InspectorClientBlackBerry::populateSetting(const String& key, String* value)
{
    if (m_inspectorSettingsMap->contains(key))
        *value = m_inspectorSettingsMap->get(key);
}

void InspectorClientBlackBerry::storeSetting(const String& key, const String& value)
{
    m_inspectorSettingsMap->set(key, value);
}

void InspectorClientBlackBerry::inspectorWindowObjectCleared()
{
    notImplemented();
}

void InspectorClientBlackBerry::openInspectorFrontend(InspectorController*)
{
    notImplemented();
}

void InspectorClientBlackBerry::closeInspectorFrontend()
{
    notImplemented();
}

void InspectorClientBlackBerry::bringFrontendToFront()
{
    notImplemented();
}

bool InspectorClientBlackBerry::sendMessageToFrontend(const String& message)
{
    CString utf8Message = message.utf8();
    m_webPagePrivate->m_client->handleWebInspectorMessageToFrontend(0, utf8Message.data(), utf8Message.length());
    return true;
}

void InspectorClientBlackBerry::clearBrowserCache()
{
    m_webPagePrivate->m_client->clearCache();
}

void InspectorClientBlackBerry::clearBrowserCookies()
{
    m_webPagePrivate->m_client->clearCookies();
}

} // namespace WebCore
