/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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

#ifndef KSVG_SVGDocumentImpl_H
#define KSVG_SVGDocumentImpl_H
#if SVG_SUPPORT

#include <q3ptrlist.h>

#include <kdom/cache/KDOMCachedScript.h>
#include <kdom/cache/KDOMCachedObjectClient.h>
#include <kdom/core/DOMConfiguration.h>
#include <ksvg2/misc/KSVGTimeScheduler.h>

#include "Document.h"

namespace WebCore {

    class SVGElement;
    class SVGSVGElement;
    class SVGScriptElement;
    class SVGDOMImplementation;

    class SVGDocument : public Document,
                            public CachedObjectClient
    {
    public:
        SVGDocument(SVGDOMImplementation *i, FrameView *view);
        virtual ~SVGDocument();

        SVGSVGElement *rootElement() const;
        
        String title() const;
        
        virtual PassRefPtr<Element> createElement(const String& tagName, ExceptionCode&);

        // Derived from: 'CachedObjectClient'
        virtual void notifyFinished(CachedObject *finishedObj);

        FrameView *svgView() const;

        // Internal
        void finishedParsing();
        void dispatchRecursiveEvent(Event *event, Node *obj);
        void dispatchZoomEvent(float prevScale, float newScale);
        void dispatchScrollEvent();

        virtual void recalcStyle(StyleChange = NoChange);

        void addForwardReference(const SVGElement *element);

    protected:
        virtual CSSStyleSelector *createStyleSelector(const DeprecatedString &);

    private:
        void dispatchUIEvent(EventTarget *target, const AtomicString &type);
        void dispatchMouseEvent(EventTarget *target, const AtomicString &type);

        // <script> related
        void executeScripts(bool needsStyleSelectorUpdate);
        void addScripts(Node *obj);

        CachedScript *m_cachedScript;
        Q3PtrList<SVGScriptElement> m_scripts;
        Q3PtrListIterator<SVGScriptElement> *m_scriptsIt;
        Q3PtrList<SVGElement> m_forwardReferences;
    };
};

#endif // SVG_SUPPORT

#endif

// vim:ts=4:noet
