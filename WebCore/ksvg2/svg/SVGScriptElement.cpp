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
#if SVG_SUPPORT
#include "PlatformString.h"
//#include <kdom/ecma/Ecma.h>
#include <kdom/core/Attr.h>
#include "StringImpl.h"

#include "ksvg.h"
#include "SVGNames.h"
#include "SVGScriptElement.h"

using namespace WebCore;

SVGScriptElement::SVGScriptElement(const QualifiedName& tagName, Document *doc) : SVGElement(tagName, doc), SVGURIReference(), SVGExternalResourcesRequired()
{
}

SVGScriptElement::~SVGScriptElement()
{
}

StringImpl *SVGScriptElement::type() const
{
    return m_type.impl();
}

void SVGScriptElement::setType(StringImpl *type)
{
    m_type = type;
}

void SVGScriptElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == SVGNames::typeAttr)
            setType(attr->value().impl());
    else
    {
        if(SVGURIReference::parseMappedAttribute(attr)) return;
        if(SVGExternalResourcesRequired::parseMappedAttribute(attr)) return;

        SVGElement::parseMappedAttribute(attr);
    }
}

void SVGScriptElement::executeScript(Document *document, StringImpl *jsCode)
{
    if(!document || !jsCode)
        return;
#if 0
    Ecma *ecmaEngine = document->ecmaEngine();
    if(!ecmaEngine)
        return;
                
    KJS::Interpreter::lock();

    // Run script
    KJS::Completion comp = ecmaEngine->evaluate(jsCode.deprecatedString(), ecmaEngine->globalObject());
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
        SVGDocument *svgDocument = static_cast<SVGDocument *>(document);
        if(svgDocument && document->hasListenerType(ERROR_EVENT))
        {
            RefPtr<Event> event = svgDocument->createEvent("SVGEvents");
            event->initEvent(EventNames::errorEvent, false, false);
            svgDocument->dispatchRecursiveEvent(event.get(), svgDocument->lastChild());
        }

        kdDebug() << "[SVGScriptElement] Evaluation error, line " << (lineno != -1 ? DeprecatedString::number(lineno) : DeprecatedString::fromLatin1("N/A"))  << " " << exVal->toString(exec).deprecatedString() << endl;
    }
    else if(comp.complType() == KJS::ReturnValue)
        kdDebug() << "[SVGScriptElement] Return value: " << comp.value()->toString(ecmaEngine->globalExec()).deprecatedString() << endl;
    else if(comp.complType() == KJS::Normal)
        kdDebug() << "[SVGScriptElement] Evaluated ecma script!" << endl;
    
    KJS::Interpreter::unlock();
#else
    if (jsCode)
        // Hack to close memory leak due to #if 0
        String(jsCode);
#endif
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

