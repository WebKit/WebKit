/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Samuel Weinig <sam@webkit.org>
 *  Copyright (C) 2009 Google, Inc. All rights reserved.
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

#ifndef JSDOMWrapper_h
#define JSDOMWrapper_h

#include "JSDOMGlobalObject.h"
#include <runtime/JSDestructibleObject.h>

namespace WebCore {

class ScriptExecutionContext;

static const uint8_t JSNodeType = JSC::LastJSCObjectType + 1;
static const uint8_t JSDocumentWrapperType = JSC::LastJSCObjectType + 2;
static const uint8_t JSElementType = JSC::LastJSCObjectType + 3;

class JSDOMObject : public JSC::JSDestructibleObject {
public:
    typedef JSC::JSDestructibleObject Base;
    static constexpr bool isDOMWrapper = false;

    JSDOMGlobalObject* globalObject() const { return JSC::jsCast<JSDOMGlobalObject*>(JSC::JSNonFinalObject::globalObject()); }
    ScriptExecutionContext* scriptExecutionContext() const { return globalObject()->scriptExecutionContext(); }

protected:
    JSDOMObject(JSC::Structure* structure, JSC::JSGlobalObject& globalObject) 
        : Base(globalObject.vm(), structure)
    {
        ASSERT(scriptExecutionContext());
    }
};

template<typename ImplementationClass> class JSDOMWrapper : public JSDOMObject {
public:
    typedef JSDOMObject Base;
    typedef ImplementationClass DOMWrapped;
    static constexpr bool isDOMWrapper = true;

    ImplementationClass& wrapped() const { return const_cast<ImplementationClass&>(m_wrapped.get()); }

protected:
    JSDOMWrapper(JSC::Structure* structure, JSC::JSGlobalObject& globalObject, Ref<ImplementationClass>&& impl)
        : Base(structure, globalObject)
        , m_wrapped(WTFMove(impl)) { }

private:
    Ref<ImplementationClass> m_wrapped;
};

template<typename JSClass, typename Enable = void>
struct JSDOMObjectInspector {
public:
    static constexpr bool isSimpleWrapper = false;
    static constexpr bool isComplexWrapper = false;
    static constexpr bool isBuiltin = true;
};

template<typename JSClass>
struct JSDOMObjectInspector<JSClass, typename std::enable_if<JSClass::isDOMWrapper>::type> {
private:
    template<typename T> static constexpr auto test(int) -> decltype(T::create(), bool()) { return true; }
    template<typename T> static constexpr bool test(...) { return false; }

public:
    static constexpr bool isSimpleWrapper = test<typename JSClass::DOMWrapped>(0);
    static constexpr bool isComplexWrapper = !isSimpleWrapper;
    static constexpr bool isBuiltin = false;
};

} // namespace WebCore

#endif // JSDOMWrapper_h
