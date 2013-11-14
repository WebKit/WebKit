# Copyright (c) 2013 Google Inc. All rights reserved.
# Copyright (c) 2013 Apple Inc. All Rights Reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# This file contains string resources for CodeGeneratorInspector.
# Its syntax is a Python syntax subset, suitable for manual parsing.

frontend_domain_class = (
"""class $domainClassName {
public:
    $domainClassName(InspectorFrontendChannel* inspectorFrontendChannel) : m_inspectorFrontendChannel(inspectorFrontendChannel) { }
${frontendDomainMethodDeclarations}private:
    InspectorFrontendChannel* m_inspectorFrontendChannel;
};

""")

backend_dispatcher_constructor = (
"""PassRefPtr<${dispatcherName}> ${dispatcherName}::create(InspectorBackendDispatcher* backendDispatcher, ${agentName}* agent)
{
    return adoptRef(new ${dispatcherName}(backendDispatcher, agent));
}

${dispatcherName}::${dispatcherName}(InspectorBackendDispatcher* backendDispatcher, ${agentName}* agent)
    : InspectorSupplementalBackendDispatcher(backendDispatcher)
    , m_agent(agent)
{
    m_backendDispatcher->registerDispatcherForDomain(ASCIILiteral("${domainName}"), this);
}
""")

backend_dispatcher_dispatch_method_simple = (
"""void ${dispatcherName}::dispatch(long callId, const String& method, PassRefPtr<InspectorObject> message)
{
    Ref<${dispatcherName}> protect(*this);

${ifChain}
    else
        m_backendDispatcher->reportProtocolError(&callId, InspectorBackendDispatcher::MethodNotFound, String("'") + "${domainName}" + '.' + method + "' was not found");
}
""")

backend_dispatcher_dispatch_method = (
"""void ${dispatcherName}::dispatch(long callId, const String& method, PassRefPtr<InspectorObject> message)
{
    Ref<${dispatcherName}> protect(*this);

    typedef void (${dispatcherName}::*CallHandler)(long callId, const InspectorObject& message);
    typedef HashMap<String, CallHandler> DispatchMap;
    DEFINE_STATIC_LOCAL(DispatchMap, dispatchMap, ());
    if (dispatchMap.isEmpty()) {
        static const struct MethodTable {
            const char* name;
            CallHandler handler;
        } commands[] = {
${dispatcherCommands}
        };
        size_t length = WTF_ARRAY_LENGTH(commands);
        for (size_t i = 0; i < length; ++i)
            dispatchMap.add(commands[i].name, commands[i].handler);
    }

    HashMap<String, CallHandler>::iterator it = dispatchMap.find(method);
    if (it == dispatchMap.end()) {
        m_backendDispatcher->reportProtocolError(&callId, InspectorBackendDispatcher::MethodNotFound, String("'") + "${domainName}" + '.' + method + "' was not found");
        return;
    }

    ((*this).*it->value)(callId, *message.get());
}
""")

backend_method = (
"""void ${dispatcherName}::${methodName}(long callId, const InspectorObject&${requestMessageObject})
{
${methodInParametersHandling}${methodDispatchHandling}${methodOutParametersHandling}${methodEndingHandling}
}
""")

frontend_method = ("""void Inspector${domainName}FrontendDispatcher::$eventName($parameters)
{
    RefPtr<InspectorObject> jsonMessage = InspectorObject::create();
    jsonMessage->setString("method", "$domainName.$eventName");
$code
    m_inspectorFrontendChannel->sendMessageToFrontend(jsonMessage->toJSONString());
}
""")

callback_method = (
"""${handlerName}::${callbackName}::${callbackName}(PassRefPtr<InspectorBackendDispatcher> backendDispatcher, int id) : InspectorBackendDispatcher::CallbackBase(backendDispatcher, id) { }

void ${handlerName}::${callbackName}::sendSuccess(${parameters})
{
    RefPtr<InspectorObject> jsonMessage = InspectorObject::create();
${code}    sendIfActive(jsonMessage, ErrorString());
}
""")

frontend_h = (
"""#ifndef InspectorFrontend_h
#define InspectorFrontend_h

#include "InspectorTypeBuilder.h"
#include "InspectorValues.h"
#include <wtf/PassRefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class InspectorFrontendChannel;

#if ENABLE(INSPECTOR)

$domainClassList

#endif // ENABLE(INSPECTOR)

} // namespace WebCore

#endif // !defined(InspectorFrontend_h)
""")

backend_h = (
"""#ifndef InspectorBackendDispatchers_h
#define InspectorBackendDispatchers_h

#include "InspectorBackendDispatcher.h"
#include "InspectorTypeBuilder.h"
#include <wtf/PassRefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class InspectorObject;
class InspectorArray;

typedef String ErrorString;

$handlerInterfaces

$dispatcherInterfaces
} // namespace WebCore

#endif // !defined(InspectorBackendDispatchers_h)
""")

backend_cpp = (
"""
#include "config.h"

#if ENABLE(INSPECTOR)

#include "InspectorBackendDispatchers.h"

#include "InspectorAgent.h"
#include "InspectorValues.h"
#include "InspectorFrontendChannel.h"
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

$methods
} // namespace WebCore

#endif // ENABLE(INSPECTOR)
""")

frontend_cpp = (
"""

#include "config.h"

#if ENABLE(INSPECTOR)

#include "InspectorFrontend.h"

#include "InspectorFrontendChannel.h"
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

$methods

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
""")

typebuilder_h = (
"""
#ifndef InspectorTypeBuilder_h
#define InspectorTypeBuilder_h

#if ENABLE(INSPECTOR)

#include "InspectorValues.h"
#include <wtf/Assertions.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

namespace TypeBuilder {

template<typename T>
class OptOutput {
public:
    OptOutput() : m_assigned(false) { }

    void operator=(T value)
    {
        m_value = value;
        m_assigned = true;
    }

    bool isAssigned() { return m_assigned; }

    T getValue()
    {
        ASSERT(isAssigned());
        return m_value;
    }

private:
    T m_value;
    bool m_assigned;

    WTF_MAKE_NONCOPYABLE(OptOutput);
};


// A small transient wrapper around int type, that can be used as a funciton parameter type
// cleverly disallowing C++ implicit casts from float or double.
class ExactlyInt {
public:
    template<typename T>
    ExactlyInt(T t) : m_value(cast_to_int<T>(t)) {}

    ExactlyInt() {}

    operator int() { return m_value; }
private:
    int m_value;

    template<typename T>
    static int cast_to_int(T) { return T::default_case_cast_is_not_supported(); }
};

template<>
inline int ExactlyInt::cast_to_int<int>(int i) { return i; }

template<>
inline int ExactlyInt::cast_to_int<unsigned int>(unsigned int i) { return i; }

class RuntimeCastHelper {
public:
#if $validatorIfdefName
    template<InspectorValue::Type TYPE>
    static void assertType(InspectorValue* value)
    {
        ASSERT(value->type() == TYPE);
    }
    static void assertAny(InspectorValue*);
    static void assertInt(InspectorValue* value);
#endif
};


// This class provides "Traits" type for the input type T. It is programmed using C++ template specialization
// technique. By default it simply takes "ItemTraits" type from T, but it doesn't work with the base types.
template<typename T>
struct ArrayItemHelper {
    typedef typename T::ItemTraits Traits;
};

template<typename T>
class Array : public InspectorArrayBase {
private:
    Array() { }

    InspectorArray* openAccessors() {
        COMPILE_ASSERT(sizeof(InspectorArray) == sizeof(Array<T>), cannot_cast);
        return static_cast<InspectorArray*>(static_cast<InspectorArrayBase*>(this));
    }

public:
    void addItem(PassRefPtr<T> value)
    {
        ArrayItemHelper<T>::Traits::pushRefPtr(this->openAccessors(), value);
    }

    void addItem(T value)
    {
        ArrayItemHelper<T>::Traits::pushRaw(this->openAccessors(), value);
    }

    static PassRefPtr<Array<T>> create()
    {
        return adoptRef(new Array<T>());
    }

    static PassRefPtr<Array<T>> runtimeCast(PassRefPtr<InspectorValue> value)
    {
        RefPtr<InspectorArray> array;
        bool castRes = value->asArray(&array);
        ASSERT_UNUSED(castRes, castRes);
#if $validatorIfdefName
        assertCorrectValue(array.get());
#endif  // $validatorIfdefName
        COMPILE_ASSERT(sizeof(Array<T>) == sizeof(InspectorArray), type_cast_problem);
        return static_cast<Array<T>*>(static_cast<InspectorArrayBase*>(array.get()));
    }

#if $validatorIfdefName
    static void assertCorrectValue(InspectorValue* value)
    {
        RefPtr<InspectorArray> array;
        bool castRes = value->asArray(&array);
        ASSERT_UNUSED(castRes, castRes);
        for (unsigned i = 0; i < array->length(); i++)
            ArrayItemHelper<T>::Traits::template assertCorrectValue<T>(array->get(i).get());
    }

#endif // $validatorIfdefName
};

struct StructItemTraits {
    static void pushRefPtr(InspectorArray* array, PassRefPtr<InspectorValue> value)
    {
        array->pushValue(value);
    }

#if $validatorIfdefName
    template<typename T>
    static void assertCorrectValue(InspectorValue* value) {
        T::assertCorrectValue(value);
    }
#endif  // $validatorIfdefName
};

template<>
struct ArrayItemHelper<String> {
    struct Traits {
        static void pushRaw(InspectorArray* array, const String& value)
        {
            array->pushString(value);
        }

#if $validatorIfdefName
        template<typename T>
        static void assertCorrectValue(InspectorValue* value) {
            RuntimeCastHelper::assertType<InspectorValue::TypeString>(value);
        }
#endif  // $validatorIfdefName
    };
};

template<>
struct ArrayItemHelper<int> {
    struct Traits {
        static void pushRaw(InspectorArray* array, int value)
        {
            array->pushInt(value);
        }

#if $validatorIfdefName
        template<typename T>
        static void assertCorrectValue(InspectorValue* value) {
            RuntimeCastHelper::assertInt(value);
        }
#endif  // $validatorIfdefName
    };
};

template<>
struct ArrayItemHelper<double> {
    struct Traits {
        static void pushRaw(InspectorArray* array, double value)
        {
            array->pushNumber(value);
        }

#if $validatorIfdefName
        template<typename T>
        static void assertCorrectValue(InspectorValue* value) {
            RuntimeCastHelper::assertType<InspectorValue::TypeNumber>(value);
        }
#endif  // $validatorIfdefName
    };
};

template<>
struct ArrayItemHelper<bool> {
    struct Traits {
        static void pushRaw(InspectorArray* array, bool value)
        {
            array->pushBoolean(value);
        }

#if $validatorIfdefName
        template<typename T>
        static void assertCorrectValue(InspectorValue* value) {
            RuntimeCastHelper::assertType<InspectorValue::TypeBoolean>(value);
        }
#endif  // $validatorIfdefName
    };
};

template<>
struct ArrayItemHelper<InspectorValue> {
    struct Traits {
        static void pushRefPtr(InspectorArray* array, PassRefPtr<InspectorValue> value)
        {
            array->pushValue(value);
        }

#if $validatorIfdefName
        template<typename T>
        static void assertCorrectValue(InspectorValue* value) {
            RuntimeCastHelper::assertAny(value);
        }
#endif  // $validatorIfdefName
    };
};

template<>
struct ArrayItemHelper<InspectorObject> {
    struct Traits {
        static void pushRefPtr(InspectorArray* array, PassRefPtr<InspectorValue> value)
        {
            array->pushValue(value);
        }

#if $validatorIfdefName
        template<typename T>
        static void assertCorrectValue(InspectorValue* value) {
            RuntimeCastHelper::assertType<InspectorValue::TypeObject>(value);
        }
#endif  // $validatorIfdefName
    };
};

template<>
struct ArrayItemHelper<InspectorArray> {
    struct Traits {
        static void pushRefPtr(InspectorArray* array, PassRefPtr<InspectorArray> value)
        {
            array->pushArray(value);
        }

#if $validatorIfdefName
        template<typename T>
        static void assertCorrectValue(InspectorValue* value) {
            RuntimeCastHelper::assertType<InspectorValue::TypeArray>(value);
        }
#endif  // $validatorIfdefName
    };
};

template<typename T>
struct ArrayItemHelper<TypeBuilder::Array<T>> {
    struct Traits {
        static void pushRefPtr(InspectorArray* array, PassRefPtr<TypeBuilder::Array<T>> value)
        {
            array->pushValue(value);
        }

#if $validatorIfdefName
        template<typename S>
        static void assertCorrectValue(InspectorValue* value) {
            S::assertCorrectValue(value);
        }
#endif  // $validatorIfdefName
    };
};

${forwards}

String getEnumConstantValue(int code);

${typeBuilders}
} // namespace TypeBuilder


} // namespace WebCore

#endif // ENABLE(INSPECTOR)

#endif // !defined(InspectorTypeBuilder_h)

""")

typebuilder_cpp = (
"""

#include "config.h"
#if ENABLE(INSPECTOR)

#include "InspectorTypeBuilder.h"
#include <wtf/text/CString.h>

namespace WebCore {

namespace TypeBuilder {

const char* const enum_constant_values[] = {
$enumConstantValues};

String getEnumConstantValue(int code) {
    return enum_constant_values[code];
}

} // namespace TypeBuilder

$implCode

#if $validatorIfdefName

void TypeBuilder::RuntimeCastHelper::assertAny(InspectorValue*)
{
    // No-op.
}


void TypeBuilder::RuntimeCastHelper::assertInt(InspectorValue* value)
{
    double v;
    bool castRes = value->asNumber(&v);
    ASSERT_UNUSED(castRes, castRes);
    ASSERT(static_cast<double>(static_cast<int>(v)) == v);
}

$validatorCode

#endif // $validatorIfdefName

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
""")

backend_js = (
"""

$domainInitializers
""")

param_container_access_code = """
    RefPtr<InspectorObject> paramsContainer = message.getObject("params");
    InspectorObject* paramsContainerPtr = paramsContainer.get();
    InspectorArray* protocolErrorsPtr = protocolErrors.get();
"""

class_binding_builder_part_1 = (
"""        AllFieldsSet = %s
    };

    template<int STATE>
    class Builder {
    private:
        RefPtr<InspectorObject> m_result;

        template<int STEP> Builder<STATE | STEP>& castState()
        {
            return *reinterpret_cast<Builder<STATE | STEP>*>(this);
        }

        Builder(PassRefPtr</*%s*/InspectorObject> ptr)
        {
            COMPILE_ASSERT(STATE == NoFieldsSet, builder_created_in_non_init_state);
            m_result = ptr;
        }
        friend class %s;
    public:
""")

class_binding_builder_part_2 = ("""
        Builder<STATE | %s>& set%s(%s value)
        {
            COMPILE_ASSERT(!(STATE & %s), property_%s_already_set);
            m_result->set%s("%s", %s);
            return castState<%s>();
        }
""")

class_binding_builder_part_3 = ("""
        operator RefPtr<%s>& ()
        {
            COMPILE_ASSERT(STATE == AllFieldsSet, result_is_not_ready);
            COMPILE_ASSERT(sizeof(%s) == sizeof(InspectorObject), cannot_cast);
            return *reinterpret_cast<RefPtr<%s>*>(&m_result);
        }

        PassRefPtr<%s> release()
        {
            return RefPtr<%s>(*this).release();
        }
    };

""")

class_binding_builder_part_4 = (
"""    static Builder<NoFieldsSet> create()
    {
        return Builder<NoFieldsSet>(InspectorObject::create());
    }
""")
