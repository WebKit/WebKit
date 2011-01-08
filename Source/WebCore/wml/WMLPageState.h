/*
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * Copyright (C) 2004-2007 Apple Inc. All rights reserved.
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
 *
 */

#ifndef WMLPageState_h
#define WMLPageState_h

#if ENABLE(WML)
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class Page;
class WMLCardElement;

typedef HashMap<String, String> WMLVariableMap;

class WMLPageState {
public:
    WMLPageState(Page*);
    virtual ~WMLPageState();

#ifndef NDEBUG
    void dump();
#endif

    // Reset the browser context
    void reset();

    // Variable handling
    void storeVariable(const String& name, const String& value) { m_variables.set(name, value); }
    void storeVariables(WMLVariableMap& variables) { m_variables = variables; }
    String getVariable(const String& name) const { return m_variables.get(name); }
    bool hasVariables() const { return !m_variables.isEmpty(); }

    Page* page() const { return m_page; }

    // Deck access control
    bool processAccessControlData(const String& dmain, const String& path);
    void resetAccessControlData();

    bool canAccessDeck() const;

private:
    bool hostIsAllowedToAccess(const String&) const;
    bool pathIsAllowedToAccess(const String&) const;

private:
    Page* m_page;
    WMLVariableMap m_variables;
    String m_accessDomain;
    String m_accessPath;
    bool m_hasAccessControlData;
};

}

#endif
#endif
