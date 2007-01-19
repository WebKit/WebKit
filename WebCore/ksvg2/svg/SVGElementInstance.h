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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef SVGElementInstance_h
#define SVGElementInstance_h

#ifdef SVG_SUPPORT

#include "Shared.h"

#include <wtf/RefPtr.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {
    class SVGElement;
    class SVGUseElement;
    class SVGElementInstanceList;

    class SVGElementInstance : public TreeShared<SVGElementInstance> {
    public:
        SVGElementInstance(PassRefPtr<SVGUseElement>, PassRefPtr<SVGElement> clonedElement, PassRefPtr<SVGElement> originalElement);
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
        SVGElement* clonedElement() const; 
 
    private: // Helper methods
        friend class SVGUseElement;
        void appendChild(PassRefPtr<SVGElementInstance> child);

        friend class SVGStyledElement;
        void updateInstance(SVGElement*);

    private:
        RefPtr<SVGUseElement> m_useElement;
        RefPtr<SVGElement> m_element;
        RefPtr<SVGElement> m_clonedElement;

        SVGElementInstance* m_previousSibling;
        SVGElementInstance* m_nextSibling;

        SVGElementInstance* m_firstChild;
        SVGElementInstance* m_lastChild;
    };

} // namespace WebCore

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
