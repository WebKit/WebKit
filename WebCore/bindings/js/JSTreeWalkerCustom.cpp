/*
 * Copyright (C) 2006, 2008, 2009 Apple Inc. All rights reserved.
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
#include "JSTreeWalker.h"

#include "JSNode.h"
#include "Node.h"
#include "NodeFilter.h"
#include "TreeWalker.h"

using namespace JSC;

namespace WebCore {
    
void JSTreeWalker::markChildren(MarkStack& markStack)
{
    Base::markChildren(markStack);

    if (NodeFilter* filter = m_impl->filter())
        filter->markAggregate(markStack);
}
    
JSValue JSTreeWalker::parentNode(ExecState* exec, const ArgList&)
{
    Node* node = impl()->parentNode(exec);
    if (exec->hadException())
        return jsUndefined();
    return toJS(exec, node);
}
    
JSValue JSTreeWalker::firstChild(ExecState* exec, const ArgList&)
{
    Node* node = impl()->firstChild(exec);
    if (exec->hadException())
        return jsUndefined();
    return toJS(exec, node);
}
    
JSValue JSTreeWalker::lastChild(ExecState* exec, const ArgList&)
{
    Node* node = impl()->lastChild(exec);
    if (exec->hadException())
        return jsUndefined();
    return toJS(exec, node);
}
    
JSValue JSTreeWalker::nextSibling(ExecState* exec, const ArgList&)
{
    Node* node = impl()->nextSibling(exec);
    if (exec->hadException())
        return jsUndefined();
    return toJS(exec, node);
}
    
JSValue JSTreeWalker::previousSibling(ExecState* exec, const ArgList&)
{
    Node* node = impl()->previousSibling(exec);
    if (exec->hadException())
        return jsUndefined();
    return toJS(exec, node);
}
    
JSValue JSTreeWalker::previousNode(ExecState* exec, const ArgList&)
{
    Node* node = impl()->previousNode(exec);
    if (exec->hadException())
        return jsUndefined();
    return toJS(exec, node);
}
    
JSValue JSTreeWalker::nextNode(ExecState* exec, const ArgList&)
{
    Node* node = impl()->nextNode(exec);
    if (exec->hadException())
        return jsUndefined();
    return toJS(exec, node);
}

}
