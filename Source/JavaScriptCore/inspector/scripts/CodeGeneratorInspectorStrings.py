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
"""class ${exportMacro} ${domainClassName} {
public:
    ${domainClassName}(InspectorFrontendChannel* inspectorFrontendChannel) : m_inspectorFrontendChannel(inspectorFrontendChannel) { }
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

    typedef void (${dispatcherName}::*CallHandler)(long callId, const Inspector::InspectorObject& message);
    typedef HashMap<String, CallHandler> DispatchMap;
    DEPRECATED_DEFINE_STATIC_LOCAL(DispatchMap, dispatchMap, ());
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

frontend_method = ("""void Inspector${domainName}FrontendDispatcher::${eventName}(${parameters})
{
    RefPtr<InspectorObject> jsonMessage = InspectorObject::create();
    jsonMessage->setString(ASCIILiteral("method"), ASCIILiteral("${domainName}.${eventName}"));
${code}
    m_inspectorFrontendChannel->sendMessageToFrontend(jsonMessage->toJSONString());
}
""")

callback_method = (
"""${handlerName}::${callbackName}::${callbackName}(PassRefPtr<InspectorBackendDispatcher> backendDispatcher, int id) : Inspector::InspectorBackendDispatcher::CallbackBase(backendDispatcher, id) { }

void ${handlerName}::${callbackName}::sendSuccess(${parameters})
{
    RefPtr<InspectorObject> jsonMessage = InspectorObject::create();
${code}    sendIfActive(jsonMessage, ErrorString());
}
""")

frontend_h = (
"""#ifndef Inspector${outputFileNamePrefix}FrontendDispatchers_h
#define Inspector${outputFileNamePrefix}FrontendDispatchers_h

#include "Inspector${outputFileNamePrefix}TypeBuilders.h"
#include <inspector/InspectorFrontendChannel.h>
#include <inspector/InspectorValues.h>
#include <wtf/PassRefPtr.h>
#include <wtf/text/WTFString.h>

namespace Inspector {

#if ENABLE(INSPECTOR)

${domainClassList}

#endif // ENABLE(INSPECTOR)

} // namespace Inspector

#endif // !defined(Inspector${outputFileNamePrefix}FrontendDispatchers_h)
""")

backend_h = (
"""#ifndef Inspector${outputFileNamePrefix}BackendDispatchers_h
#define Inspector${outputFileNamePrefix}BackendDispatchers_h

#if ENABLE(INSPECTOR)

#include "Inspector${outputFileNamePrefix}TypeBuilders.h"
#include <inspector/InspectorBackendDispatcher.h>
#include <wtf/PassRefPtr.h>
#include <wtf/text/WTFString.h>

namespace Inspector {

typedef String ErrorString;

${handlerInterfaces}

${dispatcherInterfaces}
} // namespace Inspector

#endif // ENABLE(INSPECTOR)

#endif // !defined(Inspector${outputFileNamePrefix}BackendDispatchers_h)
""")

backend_cpp = (
"""
#include "config.h"

#if ENABLE(INSPECTOR)

#include "Inspector${outputFileNamePrefix}BackendDispatchers.h"

#include <inspector/InspectorFrontendChannel.h>
#include <inspector/InspectorValues.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace Inspector {

${handlerImplementations}

${methods}
} // namespace Inspector

#endif // ENABLE(INSPECTOR)
""")

frontend_cpp = (
"""

#include "config.h"

#if ENABLE(INSPECTOR)

#include "Inspector${outputFileNamePrefix}FrontendDispatchers.h"

#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace Inspector {

${methods}

} // namespace Inspector

#endif // ENABLE(INSPECTOR)
""")

typebuilder_h = (
"""
#ifndef Inspector${outputFileNamePrefix}TypeBuilders_h
#define Inspector${outputFileNamePrefix}TypeBuilders_h

#if ENABLE(INSPECTOR)

#include <inspector/InspectorTypeBuilder.h>
${typeBuilderDependencies}
#include <wtf/Assertions.h>
#include <wtf/PassRefPtr.h>

namespace Inspector {

namespace TypeBuilder {

${forwards}

${exportMacro} String get${outputFileNamePrefix}EnumConstantValue(int code);

${typeBuilders}
} // namespace TypeBuilder

} // namespace Inspector

#endif // ENABLE(INSPECTOR)

#endif // !defined(Inspector${outputFileNamePrefix}TypeBuilders_h)

""")

typebuilder_cpp = (
"""

#include "config.h"
#include "Inspector${outputFileNamePrefix}TypeBuilders.h"

#if ENABLE(INSPECTOR)

#include <wtf/text/CString.h>

namespace Inspector {

namespace TypeBuilder {

static const char* const enum_constant_values[] = {
${enumConstantValues}};

String get${outputFileNamePrefix}EnumConstantValue(int code) {
    return enum_constant_values[code];
}

} // namespace TypeBuilder

${implCode}

#if ${validatorIfdefName}

${validatorCode}

#endif // ${validatorIfdefName}

} // namespace Inspector

#endif // ENABLE(INSPECTOR)
""")

backend_js = (
"""

${domainInitializers}
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
        RefPtr<Inspector::InspectorObject> m_result;

        template<int STEP> Builder<STATE | STEP>& castState()
        {
            return *reinterpret_cast<Builder<STATE | STEP>*>(this);
        }

        Builder(PassRefPtr</*%s*/Inspector::InspectorObject> ptr)
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
            m_result->set%s(ASCIILiteral("%s"), %s);
            return castState<%s>();
        }
""")

class_binding_builder_part_3 = ("""
        operator RefPtr<%s>& ()
        {
            COMPILE_ASSERT(STATE == AllFieldsSet, result_is_not_ready);
            COMPILE_ASSERT(sizeof(%s) == sizeof(Inspector::InspectorObject), cannot_cast);
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
        return Builder<NoFieldsSet>(Inspector::InspectorObject::create());
    }
""")
