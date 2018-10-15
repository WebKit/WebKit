/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2006, 2008-2009, 2013, 2016 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Samuel Weinig <sam@webkit.org>
 *  Copyright (C) 2009 Google, Inc. All rights reserved.
 *  Copyright (C) 2012 Ericsson AB. All rights reserved.
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

#pragma once

#include "ExceptionOr.h"
#include <wtf/Forward.h>

namespace JSC {
class ExecState;
}

namespace WebCore {

class DOMWindow;
class Frame;
class Node;

void printErrorMessageForFrame(Frame*, const String& message);

enum SecurityReportingOption { DoNotReportSecurityError, LogSecurityError, ThrowSecurityError };

namespace BindingSecurity {

template<typename T> T* checkSecurityForNode(JSC::ExecState&, T&);
template<typename T> T* checkSecurityForNode(JSC::ExecState&, T*);
template<typename T> ExceptionOr<T*> checkSecurityForNode(JSC::ExecState&, ExceptionOr<T*>&&);
template<typename T> ExceptionOr<T*> checkSecurityForNode(JSC::ExecState&, ExceptionOr<T&>&&);

bool shouldAllowAccessToDOMWindow(JSC::ExecState*, DOMWindow&, SecurityReportingOption = LogSecurityError);
bool shouldAllowAccessToDOMWindow(JSC::ExecState&, DOMWindow&, String& message);
bool shouldAllowAccessToDOMWindow(JSC::ExecState*, DOMWindow*, SecurityReportingOption = LogSecurityError);
bool shouldAllowAccessToDOMWindow(JSC::ExecState&, DOMWindow*, String& message);
bool shouldAllowAccessToFrame(JSC::ExecState*, Frame*, SecurityReportingOption = LogSecurityError);
bool shouldAllowAccessToFrame(JSC::ExecState&, Frame&, String& message);
bool shouldAllowAccessToNode(JSC::ExecState&, Node*);

}

template<typename T> inline T* BindingSecurity::checkSecurityForNode(JSC::ExecState& state, T& node)
{
    return shouldAllowAccessToNode(state, &node) ? &node : nullptr;
}

template<typename T> inline T* BindingSecurity::checkSecurityForNode(JSC::ExecState& state, T* node)
{
    return shouldAllowAccessToNode(state, node) ? node : nullptr;
}

template<typename T> inline ExceptionOr<T*> BindingSecurity::checkSecurityForNode(JSC::ExecState& state, ExceptionOr<T*>&& value)
{
    if (value.hasException())
        return value.releaseException();
    return checkSecurityForNode(state, value.releaseReturnValue());
}

template<typename T> inline ExceptionOr<T*> BindingSecurity::checkSecurityForNode(JSC::ExecState& state, ExceptionOr<T&>&& value)
{
    if (value.hasException())
        return value.releaseException();
    return checkSecurityForNode(state, value.releaseReturnValue());
}

} // namespace WebCore
