/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004-2011, 2013, 2016 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Samuel Weinig <sam@webkit.org>
 *  Copyright (C) 2013 Michael Pruett <michael@68k.org>
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
#include "JSDOMBindingSecurity.h"

#include "DOMWindow.h"
#include "Document.h"
#include "Frame.h"
#include "HTTPParsers.h"
#include "JSDOMExceptionHandling.h"
#include "JSDOMWindowBase.h"
#include "SecurityOrigin.h"
#include <wtf/text/WTFString.h>


namespace WebCore {
using namespace JSC;

void printErrorMessageForFrame(Frame* frame, const String& message)
{
    if (!frame)
        return;
    frame->document()->domWindow()->printErrorMessage(message);
}

static inline bool canAccessDocument(JSC::ExecState* state, Document* targetDocument, SecurityReportingOption reportingOption)
{
    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!targetDocument)
        return false;

    if (auto* templateHost = targetDocument->templateDocumentHost())
        targetDocument = templateHost;

    DOMWindow& active = activeDOMWindow(*state);

    if (active.document()->securityOrigin().canAccess(targetDocument->securityOrigin()))
        return true;

    switch (reportingOption) {
    case ThrowSecurityError:
        throwSecurityError(*state, scope, targetDocument->domWindow()->crossDomainAccessErrorMessage(active, IncludeTargetOrigin::No));
        break;
    case LogSecurityError:
        printErrorMessageForFrame(targetDocument->frame(), targetDocument->domWindow()->crossDomainAccessErrorMessage(active, IncludeTargetOrigin::Yes));
        break;
    case DoNotReportSecurityError:
        break;
    }

    return false;
}

bool BindingSecurity::shouldAllowAccessToFrame(ExecState& state, Frame& frame, String& message)
{
    if (BindingSecurity::shouldAllowAccessToFrame(&state, &frame, DoNotReportSecurityError))
        return true;
    message = frame.document()->domWindow()->crossDomainAccessErrorMessage(activeDOMWindow(state), IncludeTargetOrigin::No);
    return false;
}

bool BindingSecurity::shouldAllowAccessToDOMWindow(ExecState& state, DOMWindow* globalObject, String& message)
{
    return globalObject && shouldAllowAccessToDOMWindow(state, *globalObject, message);
}

bool BindingSecurity::shouldAllowAccessToDOMWindow(ExecState& state, DOMWindow& globalObject, String& message)
{
    if (BindingSecurity::shouldAllowAccessToDOMWindow(&state, globalObject, DoNotReportSecurityError))
        return true;
    message = globalObject.crossDomainAccessErrorMessage(activeDOMWindow(state), IncludeTargetOrigin::No);
    return false;
}

bool BindingSecurity::shouldAllowAccessToDOMWindow(JSC::ExecState* state, DOMWindow& target, SecurityReportingOption reportingOption)
{
    return canAccessDocument(state, target.document(), reportingOption);
}

bool BindingSecurity::shouldAllowAccessToDOMWindow(JSC::ExecState* state, DOMWindow* target, SecurityReportingOption reportingOption)
{
    return target && shouldAllowAccessToDOMWindow(state, *target, reportingOption);
}

bool BindingSecurity::shouldAllowAccessToFrame(JSC::ExecState* state, Frame* target, SecurityReportingOption reportingOption)
{
    return target && canAccessDocument(state, target->document(), reportingOption);
}

bool BindingSecurity::shouldAllowAccessToNode(JSC::ExecState& state, Node* target)
{
    return !target || canAccessDocument(&state, &target->document(), LogSecurityError);
}

} // namespace WebCore
