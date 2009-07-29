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

#ifndef WMLTimerElement_h
#define WMLTimerElement_h

#if ENABLE(WML)
#include "Timer.h"
#include "WMLElement.h"

namespace WebCore {

class WMLCardElement;

class WMLTimerElement : public WMLElement {
public:
    WMLTimerElement(const QualifiedName& tagName, Document*);

    virtual void parseMappedAttribute(MappedAttribute*);
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();

    void timerFired(Timer<WMLTimerElement>*);

    void start(int interval = -1);
    void stop();
    void storeIntervalToPageState();

    String value() const;

private:
    WMLCardElement* m_card;
    String m_name;
    Timer<WMLTimerElement> m_timer;
};

}

#endif
#endif
