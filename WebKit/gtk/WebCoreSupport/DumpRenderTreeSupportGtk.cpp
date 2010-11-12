/*
 *  Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
#include "DumpRenderTreeSupportGtk.h"

#include "APICast.h"
#include "Document.h"
#include "JSDocument.h"
#include "JSLock.h"
#include "JSNodeList.h"
#include "JSValue.h"
#include "NodeList.h"
#include "webkitprivate.h"
#include "webkitwebview.h"

using namespace JSC;
using namespace WebCore;

bool DumpRenderTreeSupportGtk::s_drtRun = false;
bool DumpRenderTreeSupportGtk::s_linksIncludedInTabChain = true;

DumpRenderTreeSupportGtk::DumpRenderTreeSupportGtk()
{
}

DumpRenderTreeSupportGtk::~DumpRenderTreeSupportGtk()
{
}

void DumpRenderTreeSupportGtk::setDumpRenderTreeModeEnabled(bool enabled)
{
    s_drtRun = enabled;
}

bool DumpRenderTreeSupportGtk::dumpRenderTreeModeEnabled()
{
    return s_drtRun;
}
void DumpRenderTreeSupportGtk::setLinksIncludedInFocusChain(bool enabled)
{
    s_linksIncludedInTabChain = enabled;
}

bool DumpRenderTreeSupportGtk::linksIncludedInFocusChain()
{
    return s_linksIncludedInTabChain;
}

JSValueRef DumpRenderTreeSupportGtk::nodesFromRect(JSContextRef context, JSValueRef value, int x, int y, unsigned top, unsigned right, unsigned bottom, unsigned left, bool ignoreClipping)
{
    JSLock lock(SilenceAssertionsOnly);
    ExecState* exec = toJS(context);
    if (!value)
        return JSValueMakeUndefined(context);
    JSValue jsValue = toJS(exec, value);
    if (!jsValue.inherits(&JSDocument::s_info))
       return JSValueMakeUndefined(context);

    JSDocument* jsDocument = static_cast<JSDocument*>(asObject(jsValue));
    Document* document = jsDocument->impl();
    RefPtr<NodeList> nodes = document->nodesFromRect(x, y, top, right, bottom, left, ignoreClipping);
    return toRef(exec, toJS(exec, jsDocument->globalObject(), nodes.get()));
}
