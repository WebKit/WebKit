/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "JSNamedNodesCollection.h"

#include "AtomicString.h"
#include "NamedAttrMap.h"
#include "Node.h"
#include "kjs_dom.h"

namespace WebCore {

using namespace KJS;

const ClassInfo JSNamedNodesCollection::info = { "Collection", 0, 0 };

// Such a collection is usually very short-lived, it only exists
// for constructs like document.forms.<name>[1],
// so it shouldn't be a problem that it's storing all the nodes (with the same name). (David)
JSNamedNodesCollection::JSNamedNodesCollection(KJS::JSObject* prototype, const Vector<RefPtr<Node> >& nodes)
    : KJS::DOMObject(prototype)
    , m_nodes(nodes)
{
}

JSValue* JSNamedNodesCollection::lengthGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    JSNamedNodesCollection* thisObj = static_cast<JSNamedNodesCollection*>(slot.slotBase());
    return jsNumber(thisObj->m_nodes.size());
}

JSValue* JSNamedNodesCollection::indexGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    JSNamedNodesCollection *thisObj = static_cast<JSNamedNodesCollection*>(slot.slotBase());
    return toJS(exec, thisObj->m_nodes[slot.index()].get());
}

bool JSNamedNodesCollection::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    if (propertyName == exec->propertyNames().length) {
        slot.setCustom(this, lengthGetter);
        return true;
    }

    bool ok;
    unsigned index = propertyName.toUInt32(&ok);
    if (ok && index < m_nodes.size()) {
        slot.setCustomIndex(this, index, indexGetter);
        return true;
    }

    // For IE compatibility, we need to be able to look up elements in a
    // document.formName.name result by id as well as be index.

    AtomicString atomicPropertyName = propertyName;
    for (unsigned i = 0; i < m_nodes.size(); i++) {
        Node* node = m_nodes[i].get();
        if (node->hasAttributes() && node->attributes()->id() == atomicPropertyName) {
            slot.setCustomIndex(this, i, indexGetter);
            return true;
        }
    }

    return DOMObject::getOwnPropertySlot(exec, propertyName, slot);
}

} // namespace WebCore
