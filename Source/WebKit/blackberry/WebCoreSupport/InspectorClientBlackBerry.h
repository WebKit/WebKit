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

#ifndef InspectorClientBlackBerry_h
#define InspectorClientBlackBerry_h

#include "InspectorClient.h"
#include "PlatformString.h"
#include <wtf/HashMap.h>

namespace BlackBerry {
namespace WebKit {
class WebPagePrivate;
}
}

namespace WebCore {

class InspectorClientBlackBerry : public InspectorClient {
public:
    InspectorClientBlackBerry(BlackBerry::WebKit::WebPagePrivate*);
    virtual void inspectorDestroyed();
    virtual Page* createPage();
    virtual String localizedStringsURL();
    virtual String hiddenPanels();
    virtual void showWindow();
    virtual void closeWindow();
    virtual void attachWindow();
    virtual void detachWindow();
    virtual void setAttachedWindowHeight(unsigned);
    virtual void highlight();
    virtual void hideHighlight();
    virtual void inspectedURLChanged(const String&);
    virtual void populateSetting(const String& key, String* value);
    virtual void storeSetting(const String& key, const String& value);
    virtual void inspectorWindowObjectCleared();
    virtual void openInspectorFrontend(InspectorController*);
    virtual void closeInspectorFrontend();
    virtual void bringFrontendToFront();
    virtual bool sendMessageToFrontend(const String&);
    virtual void clearBrowserCache();
    virtual bool canClearBrowserCache() { return true; }
    virtual void clearBrowserCookies();
    virtual bool canClearBrowserCookies() { return true; }

private:
    BlackBerry::WebKit::WebPagePrivate* m_webPagePrivate;
    typedef HashMap<String, String> SettingsMap;
    OwnPtr<SettingsMap> m_inspectorSettingsMap;
};

} // namespace WebCore

#endif // InspectorClientBlackBerry_h
