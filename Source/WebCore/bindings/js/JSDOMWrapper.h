/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2018 Apple Inc. All rights reserved.
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

#pragma once

#include "JSDOMGlobalObject.h"
#include "NodeConstants.h"
#include <JavaScriptCore/JSDestructibleObject.h>

namespace WebCore {

class ScriptExecutionContext;

// JSC allows us to extend JSType. If the highest bit is set, we can add any Object types and they are
// recognized as OtherObj in JSC. And we encode Node type into JSType if the given JSType is subclass of Node.
// offset | 7 | 6 | 5   4 | 3   2   1   0  |
// value  | 1 | 0 |  Non-node DOM types    |
// If the given JSType is a subclass of Node, the format is the following.
// offset | 7 | 6 | 5   4 | 3   2   1   0  |
// value  | 1 | 1 |  Kind |       NodeType |
static const uint8_t JSNodeTypeMask                  = 0b00001111;

static const uint8_t JSDOMWrapperType                = 0b10000000;
static const uint8_t JSEventType                     = 0b10000001;
static const uint8_t JSNodeType                      = 0b11000000;
static const uint8_t JSTextNodeType                  = JSNodeType | NodeConstants::TEXT_NODE;
static const uint8_t JSProcessingInstructionNodeType = JSNodeType | NodeConstants::PROCESSING_INSTRUCTION_NODE;
static const uint8_t JSDocumentTypeNodeType          = JSNodeType | NodeConstants::DOCUMENT_TYPE_NODE;
static const uint8_t JSDocumentFragmentNodeType      = JSNodeType | NodeConstants::DOCUMENT_FRAGMENT_NODE;
static const uint8_t JSDocumentWrapperType           = JSNodeType | NodeConstants::DOCUMENT_NODE;
static const uint8_t JSCommentNodeType               = JSNodeType | NodeConstants::COMMENT_NODE;
static const uint8_t JSCDATASectionNodeType          = JSNodeType | NodeConstants::CDATA_SECTION_NODE;
static const uint8_t JSAttrNodeType                  = JSNodeType | NodeConstants::ATTRIBUTE_NODE;
static const uint8_t JSElementType                   = 0b11010000 | NodeConstants::ELEMENT_NODE;

static_assert(JSDOMWrapperType > JSC::LastJSCObjectType, "JSC::JSType offers the highest bit.");
static_assert(NodeConstants::LastNodeType <= JSNodeTypeMask, "NodeType should be represented in 4bit.");

class JSDOMObject : public JSC::JSDestructibleObject {
public:
    typedef JSC::JSDestructibleObject Base;
    static constexpr bool isDOMWrapper = false;

    JSDOMGlobalObject* globalObject() const { return JSC::jsCast<JSDOMGlobalObject*>(JSC::JSNonFinalObject::globalObject()); }
    ScriptExecutionContext* scriptExecutionContext() const { return globalObject()->scriptExecutionContext(); }

protected:
    WEBCORE_EXPORT JSDOMObject(JSC::Structure*, JSC::JSGlobalObject&);
};

WEBCORE_EXPORT JSC::CompleteSubspace* outputConstraintSubspaceFor(JSC::VM&);
WEBCORE_EXPORT JSC::CompleteSubspace* globalObjectOutputConstraintSubspaceFor(JSC::VM&);

template<typename ImplementationClass> class JSDOMWrapper : public JSDOMObject {
public:
    typedef JSDOMObject Base;
    typedef ImplementationClass DOMWrapped;
    static constexpr bool isDOMWrapper = true;
    
    ImplementationClass& wrapped() const { return m_wrapped; }
    static ptrdiff_t offsetOfWrapped() { return OBJECT_OFFSETOF(JSDOMWrapper<ImplementationClass>, m_wrapped); }

protected:
    JSDOMWrapper(JSC::Structure* structure, JSC::JSGlobalObject& globalObject, Ref<ImplementationClass>&& impl)
        : Base(structure, globalObject)
        , m_wrapped(WTFMove(impl)) { }

private:
    Ref<ImplementationClass> m_wrapped;
};

template<typename ImplementationClass> struct JSDOMWrapperConverterTraits;

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

JSC::JSValue cloneAcrossWorlds(JSC::ExecState&, const JSDOMObject& owner, JSC::JSValue);

} // namespace WebCore
