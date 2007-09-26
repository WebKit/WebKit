/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "JSHTMLCollection.h"

#include "AtomicString.h"
#include "HTMLCollection.h"
#include "HTMLOptionsCollection.h"
#include "JSHTMLAllCollection.h"
#include "JSHTMLOptionsCollection.h"
#include "JSNamedNodesCollection.h"
#include "JSNode.h"
#include "Node.h"
#include "kjs_binding.h"
#include "kjs_html.h"
#include <wtf/Vector.h>

namespace WebCore {

static KJS::JSValue* getNamedItems(KJS::ExecState* exec, HTMLCollection* impl, const KJS::Identifier& propertyName)
{
    Vector<RefPtr<Node> > namedItems;
    impl->namedItems(propertyName, namedItems);

    if (namedItems.isEmpty())
        return KJS::jsUndefined();

    if (namedItems.size() == 1)
        return toJS(exec, namedItems[0].get());

    return new JSNamedNodesCollection(exec, namedItems);
}

// HTMLCollections are strange objects, they support both get and call,
// so that document.forms.item(0) and document.forms(0) both work.
KJS::JSValue* JSHTMLCollection::callAsFunction(KJS::ExecState* exec, KJS::JSObject*, const KJS::List& args)
{
    if (args.size() < 1)
        return KJS::jsUndefined();

    // Do not use thisObj here. It can be the JSHTMLDocument, in the document.forms(i) case.
    HTMLCollection* collection = impl();

    // Also, do we need the TypeError test here ?

    if (args.size() == 1) {
        // Support for document.all(<index>) etc.
        bool ok;
        KJS::UString string = args[0]->toString(exec);
        unsigned index = string.toUInt32(&ok, false);
        if (ok)
            return toJS(exec, collection->item(index));

        // Support for document.images('<name>') etc.
        return getNamedItems(exec, collection, KJS::Identifier(string));
    }

    // The second arg, if set, is the index of the item we want
    bool ok;
    KJS::UString string = args[0]->toString(exec);
    unsigned index = args[1]->toString(exec).toUInt32(&ok, false);
    if (ok) {
        String pstr = string;
        Node* node = collection->namedItem(pstr);
        while (node) {
            if (!index)
                return toJS(exec, node);
            node = collection->nextNamedItem(pstr);
            --index;
        }
    }

    return KJS::jsUndefined();
}

bool JSHTMLCollection::implementsCall() const
{
    return true;
}

bool JSHTMLCollection::canGetItemsForName(KJS::ExecState* exec, HTMLCollection* thisObj, const KJS::Identifier& propertyName)
{
    return !getNamedItems(exec, thisObj, propertyName)->isUndefined();
}

KJS::JSValue* JSHTMLCollection::nameGetter(KJS::ExecState* exec, KJS::JSObject* originalObject, const KJS::Identifier& propertyName, const KJS::PropertySlot& slot)
{
    JSHTMLCollection* thisObj = static_cast<JSHTMLCollection*>(slot.slotBase());
    return getNamedItems(exec, thisObj->impl(), propertyName);
}

KJS::JSValue* JSHTMLCollection::item(KJS::ExecState* exec, const KJS::List& args)
{
    bool ok;
    uint32_t index = args[0]->toString(exec).toUInt32(&ok, false);
    if (ok)
        return toJS(exec, impl()->item(index));
    return getNamedItems(exec, impl(), KJS::Identifier(args[0]->toString(exec)));
}

KJS::JSValue* JSHTMLCollection::namedItem(KJS::ExecState* exec, const KJS::List& args)
{
    return getNamedItems(exec, impl(), KJS::Identifier(args[0]->toString(exec)));
}

KJS::JSValue* toJS(KJS::ExecState* exec, HTMLCollection* collection)
{
    if (!collection)
        return KJS::jsNull();

    KJS::ScriptInterpreter* interp = static_cast<KJS::ScriptInterpreter*>(exec->dynamicInterpreter());
    KJS::DOMObject* ret = interp->getDOMObject(collection);

    if (ret)
        return ret;

    switch (collection->collectionType()) {
        case HTMLCollection::SelectOptions:
            ret = new JSHTMLOptionsCollection(exec, static_cast<HTMLOptionsCollection*>(collection));
            break;
        case HTMLCollection::DocAll:
            ret = new JSHTMLAllCollection(exec, static_cast<HTMLCollection*>(collection));
            break;
        default:
            ret = new JSHTMLCollection(exec, static_cast<HTMLCollection*>(collection));
            break;
    }

    interp->putDOMObject(collection, ret);
    return ret;
}

} // namespace WebCore
