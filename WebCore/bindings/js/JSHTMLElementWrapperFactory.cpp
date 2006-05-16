/*
 *  Copyright (C) 2006 Apple Computer, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"
#include "JSHTMLElementWrapperFactory.h"

#include "HTMLBaseElement.h"
#include "HTMLCanvasElement.h"
#include "HTMLHeadElement.h"
#include "HTMLLinkElement.h"
#include "HTMLMetaElement.h"
#include "HTMLStyleElement.h"
#include "HTMLTitleElement.h"
#include "HTMLNames.h"
#include "JSHTMLBaseElement.h"
#include "JSHTMLCanvasElement.h"
#include "JSHTMLHeadElement.h"
#include "JSHTMLLinkElement.h"
#include "JSHTMLMetaElement.h"
#include "JSHTMLStyleElement.h"
#include "JSHTMLTitleElement.h"
#include "kjs_html.h"

using namespace KJS;

namespace WebCore {

using namespace HTMLNames;

typedef DOMNode* (*CreateHTMLElementWrapperFunction)(ExecState*, PassRefPtr<HTMLElement>);

static DOMNode* createBaseWrapper(ExecState* exec, PassRefPtr<HTMLElement> element)
{
    return new JSHTMLBaseElement(exec, static_cast<HTMLBaseElement*>(element.get()));
}

static DOMNode* createCanvasWrapper(ExecState* exec, PassRefPtr<HTMLElement> element)
{
    return new JSHTMLCanvasElement(exec, static_cast<HTMLCanvasElement*>(element.get()));
}

static DOMNode* createHeadWrapper(ExecState* exec, PassRefPtr<HTMLElement> element)
{
    return new JSHTMLHeadElement(exec, static_cast<HTMLHeadElement*>(element.get()));
}

static DOMNode* createLinkWrapper(ExecState* exec, PassRefPtr<HTMLElement> element)
{
    return new JSHTMLLinkElement(exec, static_cast<HTMLLinkElement*>(element.get()));
}

static DOMNode* createMetaWrapper(ExecState* exec, PassRefPtr<HTMLElement> element)
{
    return new JSHTMLMetaElement(exec, static_cast<HTMLMetaElement*>(element.get()));
}

static DOMNode* createStyleWrapper(ExecState* exec, PassRefPtr<HTMLElement> element)
{
    return new JSHTMLStyleElement(exec, static_cast<HTMLStyleElement*>(element.get()));
}

static DOMNode* createTitleWrapper(ExecState* exec, PassRefPtr<HTMLElement> element)
{
    return new JSHTMLTitleElement(exec, static_cast<HTMLTitleElement*>(element.get()));
}

DOMNode* createJSWrapper(ExecState* exec, PassRefPtr<HTMLElement> element)
{
    static HashMap<WebCore::AtomicStringImpl*, CreateHTMLElementWrapperFunction> map;
    if (map.isEmpty()) {
        map.set(baseTag.localName().impl(), createBaseWrapper);
        map.set(canvasTag.localName().impl(), createCanvasWrapper);
        map.set(headTag.localName().impl(), createHeadWrapper);
        map.set(linkTag.localName().impl(), createLinkWrapper);
        map.set(metaTag.localName().impl(), createMetaWrapper);
        map.set(styleTag.localName().impl(), createStyleWrapper);
        map.set(titleTag.localName().impl(), createTitleWrapper);
    }
    CreateHTMLElementWrapperFunction f = map.get(element->localName().impl());
    if (f)
        return f(exec, element);
    return new KJS::JSHTMLElement(exec, element.get());
}

}
