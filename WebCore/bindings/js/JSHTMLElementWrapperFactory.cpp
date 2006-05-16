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

#include "HTMLCanvasElement.h"
#include "HTMLMetaElement.h"
#include "HTMLNames.h"
#include "JSHTMLCanvasElement.h"
#include "JSHTMLMetaElement.h"
#include "JSHTMLElement.h"
#include "kjs_html.h"

using namespace KJS;

namespace WebCore {

using namespace HTMLNames;

typedef DOMNode* (*CreateHTMLElementWrapperFunction)(ExecState*, PassRefPtr<HTMLElement>);

static DOMNode* createCanvasWrapper(ExecState* exec, PassRefPtr<HTMLElement> element)
{
    return new JSHTMLCanvasElement(exec, static_cast<HTMLCanvasElement*>(element.get()));
}

static DOMNode* createMetaWrapper(ExecState* exec, PassRefPtr<HTMLElement> element)
{
    return new JSHTMLMetaElement(exec, static_cast<HTMLMetaElement*>(element.get()));
}

DOMNode* createJSWrapper(ExecState* exec, PassRefPtr<HTMLElement> element)
{
    static HashMap<WebCore::AtomicStringImpl*, CreateHTMLElementWrapperFunction> map;
    if (map.isEmpty()) {
        map.set(canvasTag.localName().impl(), createCanvasWrapper);
        map.set(metaTag.localName().impl(), createMetaWrapper);
    }
    CreateHTMLElementWrapperFunction f = map.get(element->localName().impl());
    if (f)
        return f(exec, element);
    return new KJS::JSHTMLElement(exec, element.get());
}

}
