/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef MouseEvent_h
#define MouseEvent_h

#include "Clipboard.h"
#include "EventTargetNode.h"
#include "MouseRelatedEvent.h"

namespace WebCore {

    // Introduced in DOM Level 2
    class MouseEvent : public MouseRelatedEvent {
    public:
        MouseEvent();
        MouseEvent(const AtomicString& type, bool canBubble, bool cancelable, AbstractView* view,
                   int detail, int screenX, int screenY, int pageX, int pageY,
                   bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, unsigned short button,
                   EventTargetNode* relatedTarget, Clipboard* clipboard = 0, bool isSimulated = false);
        virtual ~MouseEvent();

        void initMouseEvent(const AtomicString& type, bool canBubble, bool cancelable, AbstractView* view,
                            int detail, int screenX, int screenY, int clientX, int clientY,
                            bool ctrlKey, bool altKey, bool shiftKey, bool metaKey,
                            unsigned short button, EventTargetNode* relatedTarget);

        // WinIE uses 1,4,2 for left/middle/right but not for click (just for mousedown/up, maybe others),
        // but we will match the standard DOM.
        unsigned short button() const { return m_button; }
        EventTargetNode* relatedTarget() const { return m_relatedTarget.get(); }
        Clipboard* clipboard() const { return m_clipboard.get(); }
        
        Node* toElement() const;
        Node* fromElement() const;

        virtual bool isMouseEvent() const;
        virtual bool isDragEvent() const;
        virtual int which() const;

    private:
        unsigned short m_button;
        RefPtr<EventTargetNode> m_relatedTarget;
        RefPtr<Clipboard> m_clipboard;
    };

} // namespace WebCore

#endif // MouseEvent_h
