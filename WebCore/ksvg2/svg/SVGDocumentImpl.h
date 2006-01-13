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

#include <q3ptrlist.h>

#include "DocumentImpl.h"
#include <kdom/cache/KDOMCachedScript.h>
#include <kdom/cache/KDOMCachedObjectClient.h>

#include <ksvg2/misc/KSVGTimeScheduler.h>

typedef KHTMLView KSVGView;
namespace KDOM {
    typedef KHTMLView KDOMView;
}

namespace KSVG
{
    class SVGElementImpl;
    class SVGSVGElementImpl;
    class SVGScriptElementImpl;
    class SVGDOMImplementationImpl;

    class SVGDocumentImpl : public KDOM::DocumentImpl,
                            public KDOM::CachedObjectClient
    {
    public:
        SVGDocumentImpl(SVGDOMImplementationImpl *i, KDOM::KDOMView *view);
        virtual ~SVGDocumentImpl();

        SVGSVGElementImpl *rootElement() const;
        
        KDOM::DOMString title() const;
        
        virtual KDOM::ElementImpl *createElement(const KDOM::DOMString& tagName, int& exceptionCode);

        // Derived from: 'CachedObjectClient'
        virtual void notifyFinished(KDOM::CachedObject *finishedObj);

        KSVGView *svgView() const;

        // Internal
#if 0
        virtual KDOM::Ecma *ecmaEngine() const;
#endif
        void finishedParsing();
        void dispatchRecursiveEvent(KDOM::EventImpl *event, KDOM::NodeImpl *obj);
        void dispatchZoomEvent(float prevScale, float newScale);
        void dispatchScrollEvent();
        bool dispatchKeyEvent(KDOM::EventTargetImpl *target, QKeyEvent *key, bool keypress);

        virtual void recalcStyle(StyleChange = NoChange);

        void addForwardReference(const SVGElementImpl *element);

    protected:
        virtual KDOM::CSSStyleSelector *createStyleSelector(const QString &);

    private:
        void dispatchUIEvent(KDOM::EventTargetImpl *target, const KDOM::AtomicString &type);
        void dispatchMouseEvent(KDOM::EventTargetImpl *target, const KDOM::AtomicString &type);

        // <script> related
        void executeScripts(bool needsStyleSelectorUpdate);
        void addScripts(KDOM::NodeImpl *obj);

        KDOM::CachedScript *m_cachedScript;
        Q3PtrList<SVGScriptElementImpl> m_scripts;
        Q3PtrListIterator<SVGScriptElementImpl> *m_scriptsIt;
        Q3PtrList<SVGElementImpl> m_forwardReferences;
    };
};

#endif

// vim:ts=4:noet
