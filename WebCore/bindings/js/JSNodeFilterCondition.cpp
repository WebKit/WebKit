/*
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2007 Apple Inc. All rights reserved.
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
#include "JSNodeFilterCondition.h"

#include "Document.h"
#include "Frame.h"
#include "JSNode.h"
#include "JSNodeFilter.h"
#include "NodeFilter.h"
#include "kjs_proxy.h"

namespace WebCore {

JSNodeFilterCondition::JSNodeFilterCondition(KJS::JSObject* filter)
    : m_filter(filter)
{
}

void JSNodeFilterCondition::mark()
{
    m_filter->mark();
}

short JSNodeFilterCondition::acceptNode(Node* filterNode) const
{
    Node* node = filterNode;
    Frame* frame = node->document()->frame();
    KJSProxy* proxy = frame->scriptProxy();
    if (proxy && m_filter->implementsCall()) {
        KJS::JSLock lock;
        KJS::ExecState* exec = proxy->globalObject()->globalExec();
        KJS::List args;
        args.append(toJS(exec, node));
        KJS::JSObject* obj = m_filter;
        KJS::JSValue* result = obj->call(exec, obj, args);
        return result->toInt32(exec);
    }

    return NodeFilter::FILTER_REJECT;
}

} // namespace WebCore
