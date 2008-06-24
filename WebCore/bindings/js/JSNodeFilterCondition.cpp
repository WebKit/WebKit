/*
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
#include "ScriptController.h"

namespace WebCore {

using namespace KJS;

// FIXME: Add takeException as a member of ExecState?
static JSValue* takeException(ExecState* exec)
{
    JSValue* exception = exec->exception();
    exec->clearException();
    return exception;
}

JSNodeFilterCondition::JSNodeFilterCondition(JSValue* filter)
    : m_filter(filter)
{
}

void JSNodeFilterCondition::mark()
{
    if (!m_filter->marked())
        m_filter->mark();
}

short JSNodeFilterCondition::acceptNode(Node* filterNode, JSValue*& exception) const
{
    // FIXME: It makes no sense for this to depend on the document being in a frame!
    Frame* frame = filterNode->document()->frame();
    if (!frame)
        return NodeFilter::FILTER_REJECT;

    JSLock lock;

    CallData callData;
    CallType callType = m_filter->getCallData(callData);
    if (callType == CallTypeNone)
        return NodeFilter::FILTER_ACCEPT;

    ExecState* exec = frame->script()->globalObject()->globalExec();
    ArgList args;
    args.append(toJS(exec, filterNode));
    if (exec->hadException()) {
        exception = takeException(exec);
        return NodeFilter::FILTER_REJECT;
    }
    JSValue* result = call(exec, m_filter, callType, callData, m_filter, args);
    if (exec->hadException()) {
        exception = takeException(exec);
        return NodeFilter::FILTER_REJECT;
    }
    int intResult = result->toInt32(exec);
    if (exec->hadException()) {
        exception = takeException(exec);
        return NodeFilter::FILTER_REJECT;
    }
    return intResult;
}

} // namespace WebCore
