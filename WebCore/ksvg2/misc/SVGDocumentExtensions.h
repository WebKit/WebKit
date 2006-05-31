/*
    Copyright (C) 2006 Apple Computer, Inc.

    This file is part of the WebKit project

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

#ifndef SVGDocumentExtensions_H
#define SVGDocumentExtensions_H

#if SVG_SUPPORT

#include <wtf/Forward.h>
#include <wtf/HashSet.h>

namespace WebCore {

class Document;
class EventListener;
class Node;
class String;
class SVGSVGElement;
class TimeScheduler;

class SVGDocumentExtensions {
public:
    SVGDocumentExtensions(Document*);
    ~SVGDocumentExtensions();
    
    PassRefPtr<EventListener> createSVGEventListener(const String& functionName, const String& code, Node*);
    
    void addTimeContainer(SVGSVGElement*);
    void removeTimeContainer(SVGSVGElement*);
    
    void startAnimations();
    void pauseAnimations();
    void unpauseAnimations();

private:
    Document* m_doc; // weak reference
    HashSet<SVGSVGElement*> m_timeContainers; // For SVG 1.2 support this will need to be made more general.

    SVGDocumentExtensions(const SVGDocumentExtensions&);
    SVGDocumentExtensions& operator=(const SVGDocumentExtensions&);
};

}

#endif // SVG_SUPPORT

#endif
