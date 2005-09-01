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

#include <qptrlist.h>

#include <kdom/impl/DocumentImpl.h>
#include <kdom/cache/KDOMCachedScript.h>
#include <kdom/cache/KDOMCachedObjectClient.h>

#include <ksvg2/core/KSVGTimeScheduler.h>

class KCanvas;
class KCanvasView;

namespace KSVG
{
    class KSVGView;
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

        // 'SVGDocumentImpl' functions
        KDOM::DOMStringImpl *title() const;
        KDOM::DOMStringImpl *referrer() const;
        KDOM::DOMStringImpl *domain() const;
        KDOM::DOMStringImpl *URL() const;

        SVGSVGElementImpl *rootElement() const;

        virtual KDOM::ElementImpl *createElement(KDOM::DOMStringImpl *tagName);
        virtual KDOM::ElementImpl *createElementNS(KDOM::DOMStringImpl *namespaceURI, KDOM::DOMStringImpl *qualifiedName);

        // 'DocumentEvent' functions
        virtual KDOM::EventImpl *createEvent(KDOM::DOMStringImpl *eventType);

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

        virtual KDOM::DOMStringImpl *defaultNS() const;

        KCanvas *canvas() const;
        KCanvasView *canvasView() const { return m_canvasView; }
        void setCanvasView(KCanvasView *canvasView) { m_canvasView = canvasView; }

        virtual void attach();
        virtual bool attached() const { return m_canvasView != 0; }

        virtual KDOM::CSSStyleSheetImpl *createCSSStyleSheet(KDOM::NodeImpl *parent, KDOM::DOMStringImpl *url) const;
        virtual KDOM::CSSStyleSheetImpl *createCSSStyleSheet(KDOM::CSSRuleImpl *ownerRule, KDOM::DOMStringImpl *url) const;
        virtual bool prepareMouseEvent(bool readonly, int x, int y, KDOM::MouseEventImpl *event);

        // Animations
        TimeScheduler *timeScheduler() const;

        virtual void recalcStyleSelector();
        virtual void recalcStyle(StyleChange = NoChange);

    protected:
        virtual KDOM::CSSStyleSelector *createStyleSelector(const QString &);

    private:
        void dispatchUIEvent(KDOM::EventTargetImpl *target, const KDOM::DOMString &type);
        void dispatchMouseEvent(KDOM::EventTargetImpl *target, const KDOM::DOMString &type);

        SVGElementImpl *createSVGElement(KDOM::DOMStringImpl *prefix, KDOM::DOMStringImpl *localName);

        KCanvasView *m_canvasView;
        KDOM::EventTargetImpl *m_lastTarget;

        TimeScheduler *m_timeScheduler;

        // <script> related
        void executeScripts(bool needsStyleSelectorUpdate);
        void addScripts(KDOM::NodeImpl *obj);

        KDOM::CachedScript *m_cachedScript;
        QPtrList<SVGScriptElementImpl> m_scripts;
        QPtrListIterator<SVGScriptElementImpl> *m_scriptsIt;
    };
};

#endif

// vim:ts=4:noet
