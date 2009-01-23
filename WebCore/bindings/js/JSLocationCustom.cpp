/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2006 Jon Shier (jshier@iastate.edu)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reseved.
 *  Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#include "config.h"
#include "JSLocationCustom.h"

#include "DOMWindow.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "JSDOMBinding.h"
#include "JSDOMWindowCustom.h"
#include "KURL.h"
#include "Location.h"
#include "ScriptController.h"
#include <runtime/PrototypeFunction.h>

using namespace JSC;

namespace WebCore {

static JSValuePtr nonCachingStaticReplaceFunctionGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot&)
{
    return new (exec) PrototypeFunction(exec, 1, propertyName, jsLocationPrototypeFunctionReplace);
}

static JSValuePtr nonCachingStaticReloadFunctionGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot&)
{
    return new (exec) PrototypeFunction(exec, 0, propertyName, jsLocationPrototypeFunctionReload);
}

static JSValuePtr nonCachingStaticAssignFunctionGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot&)
{
    return new (exec) PrototypeFunction(exec, 1, propertyName, jsLocationPrototypeFunctionAssign);
}

bool JSLocation::customGetOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    Frame* frame = impl()->frame();
    if (!frame) {
        slot.setUndefined();
        return true;
    }

    // When accessing Location cross-domain, functions are always the native built-in ones.
    // See JSDOMWindow::customGetOwnPropertySlot for additional details.

    // Our custom code is only needed to implement the Window cross-domain scheme, so if access is
    // allowed, return false so the normal lookup will take place.
    String message;
    if (allowsAccessFromFrame(exec, frame, message))
        return false;

    // Check for the few functions that we allow, even when called cross-domain.
    const HashEntry* entry = JSLocationPrototype::s_info.propHashTable(exec)->entry(exec, propertyName);
    if (entry && (entry->attributes() & Function)) {
        if (entry->function() == jsLocationPrototypeFunctionReplace) {
            slot.setCustom(this, nonCachingStaticReplaceFunctionGetter);
            return true;
        } else if (entry->function() == jsLocationPrototypeFunctionReload) {
            slot.setCustom(this, nonCachingStaticReloadFunctionGetter);
            return true;
        } else if (entry->function() == jsLocationPrototypeFunctionAssign) {
            slot.setCustom(this, nonCachingStaticAssignFunctionGetter);
            return true;
        }
    }

    // FIXME: Other implementers of the Window cross-domain scheme (Window, History) allow toString,
    // but for now we have decided not to, partly because it seems silly to return "[Object Location]" in
    // such cases when normally the string form of Location would be the URL.

    printErrorMessageForFrame(frame, message);
    slot.setUndefined();
    return true;
}

bool JSLocation::customPut(ExecState* exec, const Identifier& propertyName, JSValuePtr value, PutPropertySlot& slot)
{
    Frame* frame = impl()->frame();
    if (!frame)
        return true;

    bool sameDomainAccess = allowsAccessFromFrame(exec, frame);

    const HashEntry* entry = JSLocation::s_info.propHashTable(exec)->entry(exec, propertyName);
    if (!entry) {
        if (sameDomainAccess)
            JSObject::put(exec, propertyName, value, slot);
        return true;
    }

    // Cross-domain access to the location is allowed when assigning the whole location,
    // but not when assigning the individual pieces, since that might inadvertently
    // disclose other parts of the original location.
    if (entry->propertyPutter() != setJSLocationHref && !sameDomainAccess)
        return true;

    return false;
}

bool JSLocation::deleteProperty(ExecState* exec, const Identifier& propertyName)
{
    // Only allow deleting by frames in the same origin.
    if (!allowsAccessFromFrame(exec, impl()->frame()))
        return false;
    return Base::deleteProperty(exec, propertyName);
}

bool JSLocation::customGetPropertyNames(ExecState* exec, PropertyNameArray&)
{
    // Only allow the location object to enumerated by frames in the same origin.
    if (!allowsAccessFromFrame(exec, impl()->frame()))
        return true;
    return false;
}

static void navigateIfAllowed(ExecState* exec, Frame* frame, const KURL& url, bool lockHistory)
{
    Frame* activeFrame = asJSDOMWindow(exec->dynamicGlobalObject())->impl()->frame();
    if (!url.protocolIs("javascript") || allowsAccessFromFrame(exec, frame)) {
        bool userGesture = activeFrame->script()->processingUserGesture();
        frame->loader()->scheduleLocationChange(url.string(), activeFrame->loader()->outgoingReferrer(), lockHistory, userGesture);
    }
}

void JSLocation::setHref(ExecState* exec, JSValuePtr value)
{
    Frame* frame = impl()->frame();
    ASSERT(frame);

    Frame* activeFrame = asJSDOMWindow(exec->dynamicGlobalObject())->impl()->frame();
    if (!activeFrame)
        return;
    if (!activeFrame->loader()->shouldAllowNavigation(frame))
        return;

    KURL url = activeFrame->loader()->completeURL(value.toString(exec));
    navigateIfAllowed(exec, frame, url, false);
}

void JSLocation::setProtocol(ExecState* exec, JSValuePtr value)
{
    Frame* frame = impl()->frame();
    ASSERT(frame);

    KURL url = frame->loader()->url();
    url.setProtocol(value.toString(exec));

    navigateIfAllowed(exec, frame, url, false);
}

void JSLocation::setHost(ExecState* exec, JSValuePtr value)
{
    Frame* frame = impl()->frame();
    ASSERT(frame);

    KURL url = frame->loader()->url();
    url.setHostAndPort(value.toString(exec));

    navigateIfAllowed(exec, frame, url, false);
}

void JSLocation::setHostname(ExecState* exec, JSValuePtr value)
{
    Frame* frame = impl()->frame();
    ASSERT(frame);

    KURL url = frame->loader()->url();
    url.setHost(value.toString(exec));

    navigateIfAllowed(exec, frame, url, false);
}

void JSLocation::setPort(ExecState* exec, JSValuePtr value)
{
    Frame* frame = impl()->frame();
    ASSERT(frame);

    KURL url = frame->loader()->url();
    // FIXME: Could make this a little less ugly if String provided a toUnsignedShort function.
    const UString& portString = value.toString(exec);
    int port = charactersToInt(portString.data(), portString.size());
    if (port < 0 || port > 0xFFFF)
        port = 0;
    url.setPort(port);

    navigateIfAllowed(exec, frame, url, false);
}

void JSLocation::setPathname(ExecState* exec, JSValuePtr value)
{
    Frame* frame = impl()->frame();
    ASSERT(frame);

    KURL url = frame->loader()->url();
    url.setPath(value.toString(exec));

    navigateIfAllowed(exec, frame, url, false);
}

void JSLocation::setSearch(ExecState* exec, JSValuePtr value)
{
    Frame* frame = impl()->frame();
    ASSERT(frame);

    KURL url = frame->loader()->url();
    url.setQuery(value.toString(exec));

    navigateIfAllowed(exec, frame, url, false);
}

void JSLocation::setHash(ExecState* exec, JSValuePtr value)
{
    Frame* frame = impl()->frame();
    ASSERT(frame);

    KURL url = frame->loader()->url();
    String oldRef = url.ref();
    String str = value.toString(exec);
    if (str.startsWith("#"))
        str = str.substring(1);
    if (oldRef == str || (oldRef.isNull() && str.isEmpty()))
        return;
    url.setRef(str);

    navigateIfAllowed(exec, frame, url, false);
}

JSValuePtr JSLocation::replace(ExecState* exec, const ArgList& args)
{
    Frame* frame = impl()->frame();
    if (!frame)
        return jsUndefined();

    Frame* activeFrame = asJSDOMWindow(exec->dynamicGlobalObject())->impl()->frame();
    if (!activeFrame) 
        return jsUndefined();
    if (!activeFrame->loader()->shouldAllowNavigation(frame))
        return jsUndefined();

    navigateIfAllowed(exec, frame, activeFrame->loader()->completeURL(args.at(exec, 0).toString(exec)), true);
    return jsUndefined();
}

JSValuePtr JSLocation::reload(ExecState* exec, const ArgList&)
{
    Frame* frame = impl()->frame();
    if (!frame)
        return jsUndefined();

    JSDOMWindow* window = toJSDOMWindow(frame);
    if (!window->allowsAccessFrom(exec))
        return jsUndefined();

    if (!frame->loader()->url().protocolIs("javascript") || (window && window->allowsAccessFrom(exec))) {
        bool userGesture = asJSDOMWindow(exec->dynamicGlobalObject())->impl()->frame()->script()->processingUserGesture();
        frame->loader()->scheduleRefresh(userGesture);
    }
    return jsUndefined();
}

JSValuePtr JSLocation::assign(ExecState* exec, const ArgList& args)
{
    Frame* frame = impl()->frame();
    if (!frame)
        return jsUndefined();

    Frame* activeFrame = asJSDOMWindow(exec->dynamicGlobalObject())->impl()->frame();
    if (!activeFrame)
        return jsUndefined();
    if (!activeFrame->loader()->shouldAllowNavigation(frame))
        return jsUndefined();

    // We want a new history item if this JS was called via a user gesture
    navigateIfAllowed(exec, frame, activeFrame->loader()->completeURL(args.at(exec, 0).toString(exec)), false);
    return jsUndefined();
}

JSValuePtr JSLocation::toString(ExecState* exec, const ArgList&)
{
    Frame* frame = impl()->frame();
    if (!frame)
        return jsUndefined();
    if (!allowsAccessFromFrame(exec, frame))
        return jsUndefined();

    return jsString(exec, impl()->toString());
}

} // namespace WebCore
