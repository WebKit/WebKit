/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "JSImageConstructor.h"

#include "Document.h"
#include "HTMLImageElement.h"
#include "JSNode.h"

using namespace KJS;

namespace WebCore {

const ClassInfo JSImageConstructor::s_info = { "ImageConstructor", 0, 0, 0 };

JSImageConstructor::JSImageConstructor(ExecState* exec, Document* document)
    : DOMObject(exec->lexicalGlobalObject()->objectPrototype())
    , m_document(document)
{
}

static JSObject* constructImage(ExecState* exec, JSObject* constructor, const ArgList& args)
{
    bool widthSet = false;
    bool heightSet = false;
    int width = 0;
    int height = 0;
    if (args.size() > 0) {
        widthSet = true;
        width = args[0]->toInt32(exec);
    }
    if (args.size() > 1) {
        heightSet = true;
        height = args[1]->toInt32(exec);
    }

    Document* document = static_cast<JSImageConstructor*>(constructor)->document();

    // Calling toJS on the document causes the JS document wrapper to be
    // added to the window object. This is done to ensure that JSDocument::mark
    // will be called (which will cause the image element to be marked if necessary).
    toJS(exec, document);

    RefPtr<HTMLImageElement> image = new HTMLImageElement(document);
    if (widthSet)
        image->setWidth(width);
    if (heightSet)
        image->setHeight(height);
    return static_cast<JSObject*>(toJS(exec, image.release()));
}

ConstructType JSImageConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructImage;
    return ConstructTypeNative;
}

} // namespace WebCore
