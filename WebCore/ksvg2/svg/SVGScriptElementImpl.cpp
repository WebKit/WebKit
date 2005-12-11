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

#include "config.h"
#include <kdom/DOMString.h>
//#include <kdom/ecma/Ecma.h>
#include <kdom/core/AttrImpl.h>
#include <kdom/core/DOMStringImpl.h>

#include "ksvg.h"
#include "SVGNames.h"
#include "SVGScriptElementImpl.h"

using namespace KSVG;

SVGScriptElementImpl::SVGScriptElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc) : SVGElementImpl(tagName, doc), SVGURIReferenceImpl(), SVGExternalResourcesRequiredImpl()
{
}

SVGScriptElementImpl::~SVGScriptElementImpl()
{
}

KDOM::DOMStringImpl *SVGScriptElementImpl::type() const
{
    return m_type.impl();
}

void SVGScriptElementImpl::setType(KDOM::DOMStringImpl *type)
{
    m_type = type;
}

void SVGScriptElementImpl::parseMappedAttribute(KDOM::MappedAttributeImpl *attr)
{
    if (attr->name() == SVGNames::typeAttr)
            setType(attr->value().impl());
    else
    {
        if(SVGURIReferenceImpl::parseMappedAttribute(attr)) return;
        if(SVGExternalResourcesRequiredImpl::parseMappedAttribute(attr)) return;

        SVGElementImpl::parseMappedAttribute(attr);
    }
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
    KJS::Completion comp = ecmaEngine->evaluate(jsCode.qstring(), ecmaEngine->globalObject());
    if(comp.complType() == KJS::Throw)
    {
        KJS::ExecState *exec = ecmaEngine->globalExec();
        KJS::JSValue *exVal = comp.value();

        int lineno = -1;
        if(exVal->isObject())
        {
            KJS::JSValue *lineVal = static_cast<KJS::JSObject *>(exVal)->get(exec, "line");
            if(lineVal->type() == KJS::NumberType)
                lineno = int(lineVal->toNumber(exec));
        }

        // Fire ERROR_EVENT upon errors...
        SVGDocumentImpl *svgDocument = static_cast<SVGDocumentImpl *>(document);
        if(svgDocument && document->hasListenerType(KDOM::ERROR_EVENT))
        {
            RefPtr<KDOM::EventImpl> event = svgDocument->createEvent("SVGEvents");
            event->initEvent(KDOM::EventNames::errorEvent, false, false);
            svgDocument->dispatchRecursiveEvent(event.get(), svgDocument->lastChild());
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
    if (jsCode)
        // Hack to close memory leak due to #if 0
        KDOM::DOMString(jsCode);
#endif
}

// vim:ts=4:noet
