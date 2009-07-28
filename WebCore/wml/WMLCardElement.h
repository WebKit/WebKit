/*
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#ifndef WMLCardElement_h
#define WMLCardElement_h

#if ENABLE(WML)
#include "WMLEventHandlingElement.h"

#include <wtf/Vector.h>

namespace WebCore {

class WMLTemplateElement;
class WMLTimerElement;

class WMLCardElement : public WMLElement, public WMLEventHandlingElement {
public:
    WMLCardElement(const QualifiedName&, Document*);
    virtual ~WMLCardElement();

    bool isNewContext() const { return m_isNewContext; }
    bool isOrdered() const { return m_isOrdered; }
    WMLTimerElement* eventTimer() const { return m_eventTimer; }
    WMLTemplateElement* templateElement() const { return m_template; }

    void setTemplateElement(WMLTemplateElement*);
    void setIntrinsicEventTimer(WMLTimerElement*);

    void handleIntrinsicEventIfNeeded();
    void handleDeckLevelTaskOverridesIfNeeded();

    virtual void parseMappedAttribute(MappedAttribute*);
    virtual void insertedIntoDocument();
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);

    // Switch active card in document to the one specified in the URL reference (foo.wml#mycard)
    // If the 'targetUrl' doesn't contain a reference, use the first <card> element in the document.
    static WMLCardElement* determineActiveCard(Document*);
    static WMLCardElement* findNamedCardInDocument(Document*, const String& cardName);

private:
    bool isVisible() const { return m_isVisible; }

    void showCard();
    void hideCard();

    bool m_isNewContext;
    bool m_isOrdered;
    bool m_isVisible;

    WMLTimerElement* m_eventTimer;
    WMLTemplateElement* m_template;
};

}

#endif
#endif
