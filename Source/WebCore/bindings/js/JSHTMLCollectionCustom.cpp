/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

#include "HTMLCollectionInlines.h"
#include "JSDOMBinding.h"
#include "JSHTMLAllCollection.h"
#include "JSHTMLFormControlsCollection.h"
#include "JSHTMLOptionsCollection.h"


namespace WebCore {
using namespace JSC;

JSValue toJSNewlyCreated(JSGlobalObject*, JSDOMGlobalObject* globalObject, Ref<HTMLCollection>&& collection)
{
    switch (collection->type()) {
    case CollectionType::FormControls:
        return createWrapper<HTMLFormControlsCollection>(globalObject, WTF::move(collection));
    case CollectionType::SelectOptions:
        return createWrapper<HTMLOptionsCollection>(globalObject, WTF::move(collection));
    case CollectionType::DocAll:
        return createWrapper<HTMLAllCollection>(globalObject, WTF::move(collection));
    default:
        break;
    }

    return createWrapper<HTMLCollection>(globalObject, WTF::move(collection));
}

JSValue toJS(JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* globalObject, HTMLCollection& collection)
{
    return wrap(lexicalGlobalObject, globalObject, collection);
}

} // namespace WebCore
