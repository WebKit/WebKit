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
#include "StringHash.h"

namespace WebCore {

class Page;
class String;
class WMLCardElement;

typedef HashMap<String, String> WMLVariableMap;

class WMLPageState {
public:
    WMLPageState(Page*);
    virtual ~WMLPageState();

    // reset the browser context when 'newcontext' attribute
    // of card element is performed as part of go task
    void reset();

    // variables operations 
    void storeVariable(const String& name, const String& value) { m_variables.set(name, value); }
    void storeVariables(WMLVariableMap& variables) { m_variables = variables; }
    String getVariable(const String& name) const { return m_variables.get(name); }
    bool hasVariables() const { return !m_variables.isEmpty(); }

    int historyLength() const { return m_historyLength; }
    void setHistoryLength(int length) { m_historyLength = length; }

    Page* page() const { return m_page; }

    WMLCardElement* activeCard() const { return m_activeCard; }
    void setActiveCard(WMLCardElement* card) { m_activeCard = card; }

    // deck access control
    void restrictDeckAccessToDomain(const String& domain) { m_accessDomain = domain; }
    void restrictDeckAccessToPath(const String& path) { m_accessPath = path; }
    bool hasDeckAccess() const { return m_hasDeckAccess; }
 
    bool setNeedCheckDeckAccess(bool);
    bool isDeckAccessible();

private:
    Page* m_page;
    WMLVariableMap m_variables;
    int m_historyLength;
    WMLCardElement* m_activeCard;
    String m_accessDomain;
    String m_accessPath;
    bool m_hasDeckAccess;
};

}

#endif
#endif
