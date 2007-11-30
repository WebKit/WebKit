/*
    Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SVGElementInstance_h
#define SVGElementInstance_h

#if ENABLE(SVG)

#include "EventTarget.h"

#include <wtf/RefPtr.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {
    class SVGElement;
    class SVGUseElement;
    class SVGElementInstanceList;

    class SVGElementInstance : public EventTarget {
    public:
        SVGElementInstance(SVGUseElement*, PassRefPtr<SVGElement> originalElement);
        virtual ~SVGElementInstance();

        // 'SVGElementInstance' functions
        SVGElement* correspondingElement() const;
        SVGUseElement* correspondingUseElement() const;

        SVGElementInstance* parentNode() const;
        PassRefPtr<SVGElementInstanceList> childNodes();

        SVGElementInstance* previousSibling() const;
        SVGElementInstance* nextSibling() const;

        SVGElementInstance* firstChild() const;
        SVGElementInstance* lastChild() const;

        // Internal usage only!
        SVGElement* shadowTreeElement() const; 
        void setShadowTreeElement(SVGElement*);

        // Model the TreeShared concept, integrated within EventTarget inheritance.
        virtual void refEventTarget() { ++m_refCount;  }
        virtual void derefEventTarget() { if (--m_refCount <= 0 && !m_parent) delete this; }

        bool hasOneRef() { return m_refCount == 1; }
        int refCount() const { return m_refCount; }

        void setParent(SVGElementInstance* parent) { m_parent = parent; }
        SVGElementInstance* parent() const { return m_parent; }

        // SVGElementInstance supports both toSVGElementInstance and toNode since so much mouse handling code depends on toNode returning a valid node.
        virtual EventTargetNode* toNode();
        virtual SVGElementInstance* toSVGElementInstance();

        virtual void addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture);
        virtual void removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture);
        virtual bool dispatchEvent(PassRefPtr<Event>, ExceptionCode&, bool tempEvent = false);
 
    private:
        SVGElementInstance(const SVGElementInstance&);
        SVGElementInstance& operator=(const SVGElementInstance&);

    private: // Helper methods
        friend class SVGUseElement;
        void appendChild(PassRefPtr<SVGElementInstance> child);

        friend class SVGStyledElement;
        void updateInstance(SVGElement*);

    private:
        int m_refCount;
        SVGElementInstance* m_parent;

        SVGUseElement* m_useElement;
        RefPtr<SVGElement> m_element;
        SVGElement* m_shadowTreeElement;

        SVGElementInstance* m_previousSibling;
        SVGElementInstance* m_nextSibling;

        SVGElementInstance* m_firstChild;
        SVGElementInstance* m_lastChild;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif

// vim:ts=4:noet
