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

#ifndef KDOM_Ecma_H
#define KDOM_Ecma_H

#include <kjs/interpreter.h>
#include <qvariant.h>

namespace KJS
{
    class UString;
    class ExecState;
    class ObjectImp;
    class Completion;
};

namespace KDOM
{
    class NodeImpl;
    class EventImpl;
    class CSSRuleImpl;
    class CSSValueImpl;

    class EventImpl;
    class DOMString;
    class CDFInterface;
    class DocumentImpl;
    class DOMStringImpl;
    class EventListenerImpl;
    class ScriptInterpreter;

    class Ecma
    {
    public:
        Ecma(DocumentImpl *doc);
        virtual ~Ecma();

        void setup(CDFInterface *interface);

        KJS::Completion evaluate(const KJS::UString &code, KJS::ValueImp *thisV);

        KJS::ObjectImp *globalObject() const;
        KJS::ExecState *globalExec() const;
        
        ScriptInterpreter *interpreter() const;

        // Internal, used to handle event listeners
        KJS::ObjectImp *ecmaListenerToObject(KJS::ExecState *exec, KJS::ValueImp *listener);

        EventListenerImpl *createEventListener(const DOMString &type, const DOMString &jsCode);
        EventListenerImpl *createEventListener(KJS::ExecState *exec, KJS::ValueImp *listener);
        EventListenerImpl *findEventListener(KJS::ExecState *exec, KJS::ValueImp *listener);

        void addEventListener(EventListenerImpl *listener, KJS::ObjectImp *imp);
        void removeEventListener(KJS::ObjectImp *imp);

        void finishedWithEvent(EventImpl *evt);

        // Very important function that needs to be reimplemented by any user of
        // KDOM which builds another W3C Language on our top, who needs EcmaScript
        // bindings; Example for SVG: var document; <- That's a 'KSVG::SVGDocument'
        // debug(document.createElement('svg')); should show 'KSVG::SVGSVGElement'
        // but KDOM doesn't know anything about it and will return 'KDOM::Element'
        // Here is the standard way to avoid that!
        virtual KJS::ObjectImp *inheritedGetDOMNode(KJS::ExecState *exec, NodeImpl *n);
        virtual KJS::ObjectImp *inheritedGetDOMEvent(KJS::ExecState *exec, EventImpl *e);
        virtual KJS::ObjectImp *inheritedGetDOMCSSRule(KJS::ExecState *exec, CSSRuleImpl *c);
        virtual KJS::ObjectImp *inheritedGetDOMCSSValue(KJS::ExecState *exec, CSSValueImpl *c);

    protected:
        virtual void setupDocument(DocumentImpl *doc);

    private:
        class Private;
        Private *d;
    };

    // Helpers
    KJS::ValueImp *getDOMNode(KJS::ExecState *exec, NodeImpl *n);
    KJS::ValueImp *getDOMEvent(KJS::ExecState *exec, EventImpl *e);
    KJS::ValueImp *getDOMCSSRule(KJS::ExecState *exec, CSSRuleImpl *c);
    KJS::ValueImp *getDOMCSSValue(KJS::ExecState *exec, CSSValueImpl *c);

    KJS::ValueImp *getDOMString(DOMStringImpl *str);

    DOMStringImpl *toDOMString(KJS::ExecState *exec, KJS::ValueImp *val);
    QVariant toVariant(KJS::ExecState *exec, KJS::ValueImp *val);

    // Convert between ecma values and real kdom objects
    // Example: NodeImpl *myNode = ecma_cast<NodeImpl>(exec, args[0], &toNode);
    //          AttrImpl *myAttr = ecma_cast<AttrImpl>(exec, args[1], &toAttr);
    template<class DOMObjImpl>
    DOMObjImpl *ecma_cast(KJS::ExecState *exec,
                          KJS::ValueImp *val,
                          DOMObjImpl *(*convFuncPtr)(KJS::ExecState *, const KJS::ObjectImp *))
    {
        if(!val->isObject())
            return 0;

        return convFuncPtr(exec, static_cast<KJS::ObjectImp *>(val));
    }

    // Convert between real kdom objects and ecma values
    // Example: return safe_cache<AttrImpl, AttrWrapper>(exec, myAttr);
    template<class DOMObjImpl, class DOMObjWrapper>
    KJS::ValueImp *safe_cache(KJS::ExecState *exec, DOMObjImpl *obj)
    {
        if(!obj)
            return KJS::Null();

        DOMObjWrapper *wrapper = new DOMObjWrapper(obj);
        return wrapper->cache(exec);
    }
};

#endif

// vim:ts=4:noet
