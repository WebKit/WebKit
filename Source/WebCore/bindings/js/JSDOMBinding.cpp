/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Samuel Weinig <sam@webkit.org>
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
#include "JSDOMBinding.h"

#include "DOMObjectHashTableMap.h"
#include "ExceptionCode.h"
#include "ExceptionHeaders.h"
#include "ExceptionInterfaces.h"
#include "Frame.h"
#include "JSDOMWindowCustom.h"
#include "JSExceptionBase.h"
#include "ScriptCallStack.h"
#include <runtime/DateInstance.h>
#include <runtime/Error.h>
#include <runtime/ExceptionHelpers.h>
#include <runtime/JSFunction.h>

using namespace JSC;

namespace WebCore {

ASSERT_HAS_TRIVIAL_DESTRUCTOR(DOMConstructorObject);
ASSERT_HAS_TRIVIAL_DESTRUCTOR(DOMConstructorWithDocument);

const JSC::HashTable* getHashTableForGlobalData(JSGlobalData& globalData, const JSC::HashTable* staticTable)
{
    return DOMObjectHashTableMap::mapFor(globalData).get(staticTable);
}

JSValue jsStringSlowCase(ExecState* exec, JSStringCache& stringCache, StringImpl* stringImpl)
{
    JSString* wrapper = jsString(exec, UString(stringImpl));
    stringCache.add(stringImpl, Weak<JSString>(exec->globalData(), wrapper, currentWorld(exec)->stringWrapperOwner(), stringImpl));
    return wrapper;
}

JSValue jsStringOrNull(ExecState* exec, const String& s)
{
    if (s.isNull())
        return jsNull();
    return jsString(exec, s);
}

JSValue jsOwnedStringOrNull(ExecState* exec, const String& s)
{
    if (s.isNull())
        return jsNull();
    return jsOwnedString(exec, stringToUString(s));
}

JSValue jsStringOrUndefined(ExecState* exec, const String& s)
{
    if (s.isNull())
        return jsUndefined();
    return jsString(exec, s);
}

JSValue jsStringOrFalse(ExecState* exec, const String& s)
{
    if (s.isNull())
        return jsBoolean(false);
    return jsString(exec, s);
}

JSValue jsString(ExecState* exec, const KURL& url)
{
    return jsString(exec, url.string());
}

JSValue jsStringOrNull(ExecState* exec, const KURL& url)
{
    if (url.isNull())
        return jsNull();
    return jsString(exec, url.string());
}

JSValue jsStringOrUndefined(ExecState* exec, const KURL& url)
{
    if (url.isNull())
        return jsUndefined();
    return jsString(exec, url.string());
}

JSValue jsStringOrFalse(ExecState* exec, const KURL& url)
{
    if (url.isNull())
        return jsBoolean(false);
    return jsString(exec, url.string());
}

AtomicStringImpl* findAtomicString(const Identifier& identifier)
{
    if (identifier.isNull())
        return 0;
    StringImpl* impl = identifier.impl();
    ASSERT(impl->existingHash());
    return AtomicString::find(impl->characters(), impl->length(), impl->existingHash());
}

String valueToStringWithNullCheck(ExecState* exec, JSValue value)
{
    if (value.isNull())
        return String();
    return ustringToString(value.toString(exec)->value(exec));
}

String valueToStringWithUndefinedOrNullCheck(ExecState* exec, JSValue value)
{
    if (value.isUndefinedOrNull())
        return String();
    return ustringToString(value.toString(exec)->value(exec));
}

JSValue jsDateOrNull(ExecState* exec, double value)
{
    if (!isfinite(value))
        return jsNull();
    return DateInstance::create(exec, exec->lexicalGlobalObject()->dateStructure(), value);
}

double valueToDate(ExecState* exec, JSValue value)
{
    if (value.isNumber())
        return value.asNumber();
    if (!value.inherits(&DateInstance::s_info))
        return std::numeric_limits<double>::quiet_NaN();
    return static_cast<DateInstance*>(value.toObject(exec))->internalNumber();
}

void reportException(ExecState* exec, JSValue exception)
{
    if (isTerminatedExecutionException(exception))
        return;

    UString errorMessage = exception.toString(exec)->value(exec);
    JSObject* exceptionObject = exception.toObject(exec);
    int lineNumber = exceptionObject->get(exec, Identifier(exec, "line")).toInt32(exec);
    UString exceptionSourceURL = exceptionObject->get(exec, Identifier(exec, "sourceURL")).toString(exec)->value(exec);
    exec->clearException();

    if (ExceptionBase* exceptionBase = toExceptionBase(exception))
        errorMessage = stringToUString(exceptionBase->message() + ": "  + exceptionBase->description());

    ScriptExecutionContext* scriptExecutionContext = static_cast<JSDOMGlobalObject*>(exec->lexicalGlobalObject())->scriptExecutionContext();
    ASSERT(scriptExecutionContext);

    // Crash data indicates null-dereference crashes at this point in the Safari 4 Public Beta.
    // It's harmless to return here without reporting the exception to the log and the debugger in this case.
    if (!scriptExecutionContext)
        return;

    scriptExecutionContext->reportException(ustringToString(errorMessage), lineNumber, ustringToString(exceptionSourceURL), 0);
}

void reportCurrentException(ExecState* exec)
{
    JSValue exception = exec->exception();
    exec->clearException();
    reportException(exec, exception);
}

#define TRY_TO_CREATE_EXCEPTION(interfaceName) \
    case interfaceName##Type: \
        errorObject = toJS(exec, globalObject, interfaceName::create(description)); \
        break;

void setDOMException(ExecState* exec, ExceptionCode ec)
{
    if (!ec || exec->hadException())
        return;

    // FIXME: All callers to setDOMException need to pass in the right global object
    // for now, we're going to assume the lexicalGlobalObject.  Which is wrong in cases like this:
    // frames[0].document.createElement(null, null); // throws an exception which should have the subframes prototypes.
    JSDOMGlobalObject* globalObject = deprecatedGlobalObjectForPrototype(exec);

    ExceptionCodeDescription description(ec);

    JSValue errorObject;
    switch (description.type) {
        DOM_EXCEPTION_INTERFACES_FOR_EACH(TRY_TO_CREATE_EXCEPTION)
    }

    ASSERT(errorObject);
    throwError(exec, errorObject);
}

#undef TRY_TO_CREATE_EXCEPTION

DOMWindow* activeDOMWindow(ExecState* exec)
{
    return asJSDOMWindow(exec->lexicalGlobalObject())->impl();
}

DOMWindow* firstDOMWindow(ExecState* exec)
{
    return asJSDOMWindow(exec->dynamicGlobalObject())->impl();
}

bool shouldAllowAccessToNode(ExecState* exec, Node* node)
{
    return node && shouldAllowAccessToFrame(exec, node->document()->frame());
}

bool shouldAllowAccessToFrame(ExecState* exec, Frame* frame)
{
    if (!frame)
        return false;
    JSDOMWindow* window = toJSDOMWindow(frame, currentWorld(exec));
    return window && window->allowsAccessFrom(exec);
}

bool shouldAllowAccessToFrame(ExecState* exec, Frame* frame, String& message)
{
    if (!frame)
        return false;
    JSDOMWindow* window = toJSDOMWindow(frame, currentWorld(exec));
    return window && window->allowsAccessFrom(exec, message);
}

void printErrorMessageForFrame(Frame* frame, const String& message)
{
    if (!frame)
        return;
    frame->domWindow()->printErrorMessage(message);
}

JSValue objectToStringFunctionGetter(ExecState* exec, JSValue, const Identifier& propertyName)
{
    return JSFunction::create(exec, exec->lexicalGlobalObject(), 0, propertyName, objectProtoFuncToString);
}

Structure* getCachedDOMStructure(JSDOMGlobalObject* globalObject, const ClassInfo* classInfo)
{
    JSDOMStructureMap& structures = globalObject->structures();
    return structures.get(classInfo).get();
}

Structure* cacheDOMStructure(JSDOMGlobalObject* globalObject, Structure* structure, const ClassInfo* classInfo)
{
    JSDOMStructureMap& structures = globalObject->structures();
    ASSERT(!structures.contains(classInfo));
    return structures.set(classInfo, WriteBarrier<Structure>(globalObject->globalData(), globalObject, structure)).first->second.get();
}

JSC::JSObject* toJSSequence(ExecState* exec, JSValue value, unsigned& length)
{
    JSObject* object = value.getObject();
    if (!object) {
        throwTypeError(exec);
        return 0;
    }
    JSValue lengthValue = object->get(exec, exec->propertyNames().length);
    if (exec->hadException())
        return 0;

    if (lengthValue.isUndefinedOrNull()) {
        throwTypeError(exec);
        return 0;
    }

    length = lengthValue.toUInt32(exec);
    if (exec->hadException())
        return 0;

    return object;
}

} // namespace WebCore
