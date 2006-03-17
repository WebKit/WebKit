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

#include "DocumentImpl.h"
#include <kdom/cache/KDOMCachedScript.h>
#include <kdom/cache/KDOMCachedObjectClient.h>

#include <ksvg2/misc/KSVGTimeScheduler.h>

namespace WebCore {

    class SVGElementImpl;
    class SVGSVGElementImpl;
    class SVGScriptElementImpl;
    class SVGDOMImplementationImpl;

    class SVGDocumentImpl : public DocumentImpl,
                            public CachedObjectClient
    {
    public:
        SVGDocumentImpl(SVGDOMImplementationImpl *i, FrameView *view);
        virtual ~SVGDocumentImpl();

        SVGSVGElementImpl *rootElement() const;
        
        DOMString title() const;
        
        virtual PassRefPtr<ElementImpl> createElement(const DOMString& tagName, ExceptionCode&);

        // Derived from: 'CachedObjectClient'
        virtual void notifyFinished(CachedObject *finishedObj);

        FrameView *svgView() const;

        // Internal
        void finishedParsing();
        void dispatchRecursiveEvent(EventImpl *event, NodeImpl *obj);
        void dispatchZoomEvent(float prevScale, float newScale);
        void dispatchScrollEvent();

        virtual void recalcStyle(StyleChange = NoChange);

        void addForwardReference(const SVGElementImpl *element);

    protected:
        virtual CSSStyleSelector *createStyleSelector(const QString &);

    private:
        void dispatchUIEvent(NodeImpl *target, const AtomicString &type);
        void dispatchMouseEvent(NodeImpl *target, const AtomicString &type);

        // <script> related
        void executeScripts(bool needsStyleSelectorUpdate);
        void addScripts(NodeImpl *obj);

        CachedScript *m_cachedScript;
        Q3PtrList<SVGScriptElementImpl> m_scripts;
        Q3PtrListIterator<SVGScriptElementImpl> *m_scriptsIt;
        Q3PtrList<SVGElementImpl> m_forwardReferences;
    };
};

#endif // SVG_SUPPORT

#endif

// vim:ts=4:noet
