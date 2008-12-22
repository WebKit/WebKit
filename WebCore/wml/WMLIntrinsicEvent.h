/**
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#ifndef WMLIntrinsicEvent_h
#define WMLIntrinsicEvent_h

#if ENABLE(WML)
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#include "WMLTaskElement.h"

namespace WebCore {

class Document;

class WMLIntrinsicEvent : public RefCounted<WMLIntrinsicEvent> {
public:
    static PassRefPtr<WMLIntrinsicEvent> create(Document* document, const String& targetURL)
    {
        return adoptRef(new WMLIntrinsicEvent(document, targetURL));
    }

    static PassRefPtr<WMLIntrinsicEvent> createWithTask(WMLTaskElement* taskElement)
    {
        return adoptRef(new WMLIntrinsicEvent(taskElement));
    }

    WMLTaskElement* taskElement() const { return m_taskElement.get(); }

private:
    WMLIntrinsicEvent(Document*, const String& targetURL);
    WMLIntrinsicEvent(WMLTaskElement*);

    RefPtr<WMLTaskElement> m_taskElement;
};

}

#endif
#endif
