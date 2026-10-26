/*
 * Copyright (C) 2026 Igalia S.L.
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
#include "JSWebExtensionWrapper.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "JSWebExtensionAPIEvent.h"
#include "JSWebExtensionAPIPort.h"
#include <WebCore/NotImplemented.h>

namespace WebKit {

String toSortedJSONString(JSContextRef context, JSValueRef value)
{
    // FIXME: Sort the keys like the Cocoa implementation. The WTF JSON implementation does not support sorting keys yet.
    notImplemented();
    return toJSONString(context, value);
}

// The bindings of these interfaces are only generated for Cocoa. Use classes without properties,
// so that the wrappers still keep their implementation objects alive.
static JSClassRef createClassWithoutProperties(const char* className)
{
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.className = className;
    definition.initialize = JSWebExtensionWrapper::initialize;
    definition.finalize = JSWebExtensionWrapper::finalize;
    return JSClassCreate(&definition);
}

JSClassRef JSWebExtensionAPIEvent::eventClassSingleton()
{
    notImplemented();
    SUPPRESS_UNCOUNTED_LOCAL static JSClassRef jsClass = createClassWithoutProperties("Event");
    return jsClass;
}

JSClassRef JSWebExtensionAPIPort::portClassSingleton()
{
    notImplemented();
    SUPPRESS_UNCOUNTED_LOCAL static JSClassRef jsClass = createClassWithoutProperties("Port");
    return jsClass;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
