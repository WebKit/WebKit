/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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
#include "JSNodeIterator.h"

#include "JSNode.h"
#include "Node.h"
#include "NodeFilter.h"
#include "NodeIterator.h"

using namespace JSC;

namespace WebCore {

void JSNodeIterator::mark()
{
    if (NodeFilter* filter = m_impl->filter())
        filter->mark();
    
    DOMObject::mark();
}

JSValue JSNodeIterator::nextNode(ExecState* exec, const ArgList&)
{
    ExceptionCode ec = 0;
    RefPtr<Node> node = impl()->nextNode(exec, ec);
    if (ec) {
        setDOMException(exec, ec);
        return jsUndefined();
    }

    if (exec->hadException())
        return jsUndefined();

    return toJS(exec, node.get());
}

JSValue JSNodeIterator::previousNode(ExecState* exec, const ArgList&)
{
    ExceptionCode ec = 0;
    RefPtr<Node> node = impl()->previousNode(exec, ec);
    if (ec) {
        setDOMException(exec, ec);
        return jsUndefined();
    }

    if (exec->hadException())
        return jsUndefined();

    return toJS(exec, node.get());
}

}
