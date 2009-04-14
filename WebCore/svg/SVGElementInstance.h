/*
    Copyright (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>

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
#include "SVGElement.h"
#include "TreeShared.h"

#include <wtf/RefPtr.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

    namespace Private { 
        template<class GenericNode, class GenericNodeContainer>
        void addChildNodesToDeletionQueue(GenericNode*& head, GenericNode*& tail, GenericNodeContainer* container);
    };

    class EventListener;
    class Frame;
    class SVGUseElement;
    class SVGElementInstanceList;

    // SVGElementInstance mimics Node, but without providing all its functionality
    class SVGElementInstance : public TreeShared<SVGElementInstance>,
                               public EventTarget
    {
    public:
        SVGElementInstance(SVGUseElement*, SVGElement* originalElement);
        virtual ~SVGElementInstance();

        bool needsUpdate() const { return m_needsUpdate; }
        void setNeedsUpdate(bool);

        virtual ScriptExecutionContext* scriptExecutionContext() const;

        virtual Node* toNode() { return shadowTreeElement(); }
        virtual SVGElementInstance* toSVGElementInstance() { return this; }

        virtual void addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture);
        virtual void removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture);
        virtual bool dispatchEvent(PassRefPtr<Event>, ExceptionCode&);
        const RegisteredEventListenerVector& eventListeners() const { return correspondingElement()->eventListeners(); }

        SVGElement* correspondingElement() const { return m_element.get(); }
        SVGUseElement* correspondingUseElement() const { return m_useElement; }
        SVGElement* shadowTreeElement() const { return m_shadowTreeElement.get(); }

        SVGElementInstance* parentNode() const { return parent(); }
        PassRefPtr<SVGElementInstanceList> childNodes();

        SVGElementInstance* previousSibling() const { return m_previousSibling; }
        SVGElementInstance* nextSibling() const { return m_nextSibling; }

        SVGElementInstance* firstChild() const { return m_firstChild; }
        SVGElementInstance* lastChild() const { return m_lastChild; }

        Document* ownerDocument() const { return m_element ? m_element->ownerDocument() : 0; }

        static void invalidateAllInstancesOfElement(SVGElement*);

        using TreeShared<SVGElementInstance>::ref;
        using TreeShared<SVGElementInstance>::deref;

        // EventTarget API
        EventListener* onabort() const;
        void setOnabort(PassRefPtr<EventListener>);
        EventListener* onblur() const;
        void setOnblur(PassRefPtr<EventListener>);
        EventListener* onchange() const;
        void setOnchange(PassRefPtr<EventListener>);
        EventListener* onclick() const;
        void setOnclick(PassRefPtr<EventListener>);
        EventListener* oncontextmenu() const;
        void setOncontextmenu(PassRefPtr<EventListener>);
        EventListener* ondblclick() const;
        void setOndblclick(PassRefPtr<EventListener>);
        EventListener* onerror() const;
        void setOnerror(PassRefPtr<EventListener>);
        EventListener* onfocus() const;
        void setOnfocus(PassRefPtr<EventListener>);
        EventListener* oninput() const;
        void setOninput(PassRefPtr<EventListener>);
        EventListener* onkeydown() const;
        void setOnkeydown(PassRefPtr<EventListener>);
        EventListener* onkeypress() const;
        void setOnkeypress(PassRefPtr<EventListener>);
        EventListener* onkeyup() const;
        void setOnkeyup(PassRefPtr<EventListener>);
        EventListener* onload() const;
        void setOnload(PassRefPtr<EventListener>);
        EventListener* onmousedown() const;
        void setOnmousedown(PassRefPtr<EventListener>);
        EventListener* onmousemove() const;
        void setOnmousemove(PassRefPtr<EventListener>);
        EventListener* onmouseout() const;
        void setOnmouseout(PassRefPtr<EventListener>);
        EventListener* onmouseover() const;
        void setOnmouseover(PassRefPtr<EventListener>);
        EventListener* onmouseup() const;
        void setOnmouseup(PassRefPtr<EventListener>);
        EventListener* onmousewheel() const;
        void setOnmousewheel(PassRefPtr<EventListener>);
        EventListener* onbeforecut() const;
        void setOnbeforecut(PassRefPtr<EventListener>);
        EventListener* oncut() const;
        void setOncut(PassRefPtr<EventListener>);
        EventListener* onbeforecopy() const;
        void setOnbeforecopy(PassRefPtr<EventListener>);
        EventListener* oncopy() const;
        void setOncopy(PassRefPtr<EventListener>);
        EventListener* onbeforepaste() const;
        void setOnbeforepaste(PassRefPtr<EventListener>);
        EventListener* onpaste() const;
        void setOnpaste(PassRefPtr<EventListener>);
        EventListener* ondragenter() const;
        void setOndragenter(PassRefPtr<EventListener>);
        EventListener* ondragover() const;
        void setOndragover(PassRefPtr<EventListener>);
        EventListener* ondragleave() const;
        void setOndragleave(PassRefPtr<EventListener>);
        EventListener* ondrop() const;
        void setOndrop(PassRefPtr<EventListener>);
        EventListener* ondragstart() const;
        void setOndragstart(PassRefPtr<EventListener>);
        EventListener* ondrag() const;
        void setOndrag(PassRefPtr<EventListener>);
        EventListener* ondragend() const;
        void setOndragend(PassRefPtr<EventListener>);
        EventListener* onreset() const;
        void setOnreset(PassRefPtr<EventListener>);
        EventListener* onresize() const;
        void setOnresize(PassRefPtr<EventListener>);
        EventListener* onscroll() const;
        void setOnscroll(PassRefPtr<EventListener>);
        EventListener* onsearch() const;
        void setOnsearch(PassRefPtr<EventListener>);
        EventListener* onselect() const;
        void setOnselect(PassRefPtr<EventListener>);
        EventListener* onselectstart() const;
        void setOnselectstart(PassRefPtr<EventListener>);
        EventListener* onsubmit() const;
        void setOnsubmit(PassRefPtr<EventListener>);
        EventListener* onunload() const;
        void setOnunload(PassRefPtr<EventListener>);

    private:
        friend class SVGUseElement;

        void appendChild(PassRefPtr<SVGElementInstance> child);
        void setShadowTreeElement(SVGElement*);
        void forgetWrapper();

        template<class GenericNode, class GenericNodeContainer>
        friend void appendChildToContainer(GenericNode* child, GenericNodeContainer* container);

        template<class GenericNode, class GenericNodeContainer>
        friend void removeAllChildrenInContainer(GenericNodeContainer* container);

        template<class GenericNode, class GenericNodeContainer>
        friend void Private::addChildNodesToDeletionQueue(GenericNode*& head, GenericNode*& tail, GenericNodeContainer* container);

        bool hasChildNodes() const { return m_firstChild; }

        void setFirstChild(SVGElementInstance* child) { m_firstChild = child; }
        void setLastChild(SVGElementInstance* child) { m_lastChild = child; }

        void setNextSibling(SVGElementInstance* sibling) { m_nextSibling = sibling; }
        void setPreviousSibling(SVGElementInstance* sibling) { m_previousSibling = sibling; }    

        virtual void refEventTarget() { ref(); }
        virtual void derefEventTarget() { deref(); }

    private:
        bool m_needsUpdate : 1;

        SVGUseElement* m_useElement;
        RefPtr<SVGElement> m_element;
        RefPtr<SVGElement> m_shadowTreeElement;

        SVGElementInstance* m_previousSibling;
        SVGElementInstance* m_nextSibling;

        SVGElementInstance* m_firstChild;
        SVGElementInstance* m_lastChild;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
