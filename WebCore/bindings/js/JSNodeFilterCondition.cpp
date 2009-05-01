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

#include "JSNode.h"
#include "JSNodeFilter.h"
#include "NodeFilter.h"
#include <runtime/JSLock.h>

namespace WebCore {

using namespace JSC;

ASSERT_CLASS_FITS_IN_CELL(JSNodeFilterCondition);

JSNodeFilterCondition::JSNodeFilterCondition(JSValue filter)
    : m_filter(filter)
{
}

void JSNodeFilterCondition::mark()
{
    if (!m_filter.marked())
        m_filter.mark();
}

short JSNodeFilterCondition::acceptNode(JSC::ExecState* exec, Node* filterNode) const
{
    JSLock lock(false);

    CallData callData;
    CallType callType = m_filter.getCallData(callData);
    if (callType == CallTypeNone)
        return NodeFilter::FILTER_ACCEPT;

   // The exec argument here should only be null if this was called from a
   // non-JavaScript language, and this is a JavaScript filter, and the document
   // in question is not associated with the frame. In that case, we're going to
   // behave incorrectly, and just reject nodes instead of calling the filter function.
   // To fix that we'd need to come up with a way to find a suitable JavaScript
   // execution context for the filter function to run in.
    if (!exec)
        return NodeFilter::FILTER_REJECT;

    MarkedArgumentBuffer args;
    args.append(toJS(exec, filterNode));
    if (exec->hadException())
        return NodeFilter::FILTER_REJECT;

    JSValue result = call(exec, m_filter, callType, callData, m_filter, args);
    if (exec->hadException())
        return NodeFilter::FILTER_REJECT;

    int intResult = result.toInt32(exec);
    if (exec->hadException())
        return NodeFilter::FILTER_REJECT;

    return intResult;
}

} // namespace WebCore
