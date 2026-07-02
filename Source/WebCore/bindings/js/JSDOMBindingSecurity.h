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

#include <WebCore/ExceptionOr.h>
#include <WebCore/HTMLFrameOwnerElement.h>
#include <WebCore/LocalDOMWindow.h>
#include <WebCore/RemoteFrame.h>
#include <wtf/Forward.h>

namespace JSC {
class CallFrame;
class JSGlobalObject;
}

namespace WebCore {

class DOMWindow;
class Frame;
class LocalFrame;
class Node;

void printErrorMessageForFrame(LocalFrame*, const String& message);

enum SecurityReportingOption { DoNotReportSecurityError, LogSecurityError, ThrowSecurityError };

namespace BindingSecurity {

template<typename T> T* checkSecurityForNode(JSC::JSGlobalObject&, T&);
template<typename T> T* checkSecurityForNode(JSC::JSGlobalObject&, T*);
template<typename T> T* checkSecurityForNodeWithFrameOwner(JSC::JSGlobalObject&, T*, const HTMLFrameOwnerElement&);
template<typename T> ExceptionOr<T*> checkSecurityForNode(JSC::JSGlobalObject&, ExceptionOr<T*>&&);
template<typename T> ExceptionOr<T*> checkSecurityForNode(JSC::JSGlobalObject&, ExceptionOr<T&>&&);
template<typename T> ExceptionOr<T*> checkSecurityForNodeWithDOMWindow(JSC::JSGlobalObject&, ExceptionOr<T*>&&, const DOMWindow&);

bool shouldAllowAccessToDOMWindow(JSC::JSGlobalObject*, LocalDOMWindow&, SecurityReportingOption = LogSecurityError);
bool shouldAllowAccessToDOMWindow(JSC::JSGlobalObject&, LocalDOMWindow&, String& message);
bool shouldAllowAccessToDOMWindow(JSC::JSGlobalObject*, LocalDOMWindow*, SecurityReportingOption = LogSecurityError);
bool shouldAllowAccessToDOMWindow(JSC::JSGlobalObject&, LocalDOMWindow*, String& message);
bool shouldAllowAccessToDOMWindow(JSC::JSGlobalObject*, DOMWindow&, SecurityReportingOption = LogSecurityError);
bool shouldAllowAccessToDOMWindow(JSC::JSGlobalObject&, DOMWindow&, String& message);
bool shouldAllowAccessToDOMWindow(JSC::JSGlobalObject*, DOMWindow*, SecurityReportingOption = LogSecurityError);
bool shouldAllowAccessToDOMWindow(JSC::JSGlobalObject&, DOMWindow*, String& message);
bool shouldAllowAccessToFrame(JSC::JSGlobalObject*, Frame*, SecurityReportingOption = LogSecurityError);
bool shouldAllowAccessToFrame(JSC::JSGlobalObject&, Frame&, String& message);
bool shouldAllowAccessToNode(JSC::JSGlobalObject&, Node*);

}

template<typename T> inline T* BindingSecurity::checkSecurityForNode(JSC::JSGlobalObject& lexicalGlobalObject, T& node)
{
    return shouldAllowAccessToNode(lexicalGlobalObject, &node) ? &node : nullptr;
}

template<typename T> inline T* BindingSecurity::checkSecurityForNode(JSC::JSGlobalObject& lexicalGlobalObject, T* node)
{
    return shouldAllowAccessToNode(lexicalGlobalObject, node) ? node : nullptr;
}

template<typename T> inline T* BindingSecurity::checkSecurityForNodeWithFrameOwner(JSC::JSGlobalObject& lexicalGlobalObject, T* node, const HTMLFrameOwnerElement& owner)
{
    if (node)
        return shouldAllowAccessToNode(lexicalGlobalObject, node) ? node : nullptr;

    // Perform access check to log cross-origin error if there is one, matching
    // behavior before Site Isolation.
    if (RefPtr frame = dynamicDowncast<RemoteFrame>(owner.contentFrame()))
        shouldAllowAccessToFrame(&lexicalGlobalObject, frame);
    return nullptr;
}

template<typename T> inline ExceptionOr<T*> BindingSecurity::checkSecurityForNode(JSC::JSGlobalObject& lexicalGlobalObject, ExceptionOr<T*>&& value)
{
    if (value.hasException())
        return value.releaseException();
    return checkSecurityForNode(lexicalGlobalObject, value.releaseReturnValue());
}

template<typename T> inline ExceptionOr<T*> BindingSecurity::checkSecurityForNode(JSC::JSGlobalObject& lexicalGlobalObject, ExceptionOr<T&>&& value)
{
    if (value.hasException())
        return value.releaseException();
    return checkSecurityForNode(lexicalGlobalObject, value.releaseReturnValue());
}

template<typename T> inline ExceptionOr<T*> BindingSecurity::checkSecurityForNodeWithDOMWindow(JSC::JSGlobalObject& lexicalGlobalObject, ExceptionOr<T*>&& value, const DOMWindow& window)
{
    if (value.hasException())
        return value.releaseException();

    RefPtr node = value.releaseReturnValue();
    if (node)
        return shouldAllowAccessToNode(lexicalGlobalObject, node.get()) ? node.get() : nullptr;

    // With site isolation, the owner element is in the parent's process. Explicitly
    // check the remote parent frame to log the cross-origin error.
    if (RefPtr localWindow = dynamicDowncast<LocalDOMWindow>(window)) {
        if (RefPtr frame = localWindow->frame()) {
            if (RefPtr parentFrame = dynamicDowncast<RemoteFrame>(frame->tree().parent()))
                shouldAllowAccessToFrame(&lexicalGlobalObject, parentFrame.get());
        }
    }
    return nullptr;
}

} // namespace WebCore
