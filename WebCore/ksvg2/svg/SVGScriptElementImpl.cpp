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

#include <kdom/DOMString.h>
//#include <kdom/ecma/Ecma.h>
#include <kdom/core/AttrImpl.h>
#include <kdom/core/DOMStringImpl.h>

#include "ksvg.h"
#include "svgattrs.h"
#include "ksvgevents.h"
#include "SVGEventImpl.h"
#include "SVGDocumentImpl.h"
#include "SVGScriptElementImpl.h"

using namespace KSVG;

SVGScriptElementImpl::SVGScriptElementImpl(KDOM::DocumentPtr *doc, KDOM::NodeImpl::Id id, KDOM::DOMStringImpl *prefix) : SVGElementImpl(doc, id, prefix), SVGURIReferenceImpl(), SVGExternalResourcesRequiredImpl()
{
    m_type = 0;
}

SVGScriptElementImpl::~SVGScriptElementImpl()
{
    if(m_type)
        m_type->deref();
}

KDOM::DOMStringImpl *SVGScriptElementImpl::type() const
{
    return m_type;
}

void SVGScriptElementImpl::setType(KDOM::DOMStringImpl *type)
{
    KDOM_SAFE_SET(m_type, type);
}

void SVGScriptElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
    int id = (attr->id() & NodeImpl_IdLocalMask);
    switch(id)
    {
        case ATTR_TYPE:
        {
            setType(attr->value());
            break;
        }
        default:
        {
            if(SVGURIReferenceImpl::parseAttribute(attr)) return;
            if(SVGExternalResourcesRequiredImpl::parseAttribute(attr)) return;

            SVGElementImpl::parseAttribute(attr);
        }
    };
}

void SVGScriptElementImpl::executeScript(KDOM::DocumentImpl *document, KDOM::DOMStringImpl *jsCode)
{
    if(!document || !jsCode)
        return;
#if 0
    KDOM::Ecma *ecmaEngine = document->ecmaEngine();
    if(!ecmaEngine)
        return;
                
#ifdef APPLE_CHANGES
    KJS::Interpreter::lock();
#endif

    // Run script
    KJS::Completion comp = ecmaEngine->evaluate(jsCode.string(), ecmaEngine->globalObject());
    if(comp.complType() == KJS::Throw)
    {
        KJS::ExecState *exec = ecmaEngine->globalExec();
        KJS::ValueImp *exVal = comp.value();

        int lineno = -1;
        if(exVal->isObject())
        {
            KJS::ValueImp *lineVal = static_cast<KJS::ObjectImp *>(exVal)->get(exec, "line");
            if(lineVal->type() == KJS::NumberType)
                lineno = int(lineVal->toNumber(exec));
        }

        // Fire ERROR_EVENT upon errors...
        SVGDocumentImpl *svgDocument = static_cast<SVGDocumentImpl *>(document);
        if(svgDocument && document->hasListenerType(KDOM::ERROR_EVENT))
        {
            SVGEventImpl *event = static_cast<SVGEventImpl *>(svgDocument->createEvent(KDOM::DOMString("SVGEvents").handle()));
            event->ref();

            event->initEvent(KDOM::DOMString("error").handle(), false, false);
            svgDocument->dispatchRecursiveEvent(event, svgDocument->lastChild());

            event->deref();
        }

        kdDebug() << "[SVGScriptElement] Evaluation error, line " << (lineno != -1 ? QString::number(lineno) : QString::fromLatin1("N/A"))  << " " << exVal->toString(exec).qstring() << endl;
    }
    else if(comp.complType() == KJS::ReturnValue)
        kdDebug() << "[SVGScriptElement] Return value: " << comp.value()->toString(ecmaEngine->globalExec()).qstring() << endl;
    else if(comp.complType() == KJS::Normal)
        kdDebug() << "[SVGScriptElement] Evaluated ecma script!" << endl;
    
#ifdef APPLE_CHANGES
    KJS::Interpreter::unlock();
#endif
#else
    if (jsCode) {
        // Hack to close memory leak due to #if 0
        jsCode->ref();
        jsCode->deref();
    }
#endif
}

// vim:ts=4:noet
