#!/usr/bin/env python
#
# Copyright (c) 2014 Apple Inc. All rights reserved.
# Copyright (c) 2014 University of Washington. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

# Generator templates, which can be filled with string.Template.
# Following are classes that fill the templates from the typechecked model.

class CppGeneratorTemplates:

    HeaderPrelude = (
    """#ifndef ${headerGuardString}
#define ${headerGuardString}

${includes}

namespace Inspector {

${typedefs}""")

    HeaderPostlude = (
    """} // namespace Inspector

#endif // !defined(${headerGuardString})""")

    ImplementationPrelude = (
    """#include "config.h"
#include ${primaryInclude}

${secondaryIncludes}

namespace Inspector {""")

    ImplementationPostlude = (
    """} // namespace Inspector
""")

    AlternateDispatchersHeaderPrelude = (
    """#ifndef ${headerGuardString}
#define ${headerGuardString}

#if ENABLE(INSPECTOR_ALTERNATE_DISPATCHERS)

${includes}

namespace Inspector {

class AlternateInspectorBackendDispatcher {
public:
    void setBackendDispatcher(RefPtr<InspectorBackendDispatcher>&& dispatcher) { m_backendDispatcher = WTF::move(dispatcher); }
    InspectorBackendDispatcher* backendDispatcher() const { return m_backendDispatcher.get(); }
private:
    RefPtr<InspectorBackendDispatcher> m_backendDispatcher;
};
""")

    AlternateDispatchersHeaderPostlude = (
    """} // namespace Inspector

#endif // ENABLE(INSPECTOR_ALTERNATE_DISPATCHERS)

#endif // !defined(${headerGuardString})""")

    AlternateBackendDispatcherHeaderDomainHandlerInterfaceDeclaration = (
    """class AlternateInspector${domainName}BackendDispatcher : public AlternateInspectorBackendDispatcher {
public:
    virtual ~AlternateInspector${domainName}BackendDispatcher() { }
${commandDeclarations}
};""")

    BackendDispatcherHeaderDomainHandlerDeclaration = (
    """${classAndExportMacro} Inspector${domainName}BackendDispatcherHandler {
public:
${commandDeclarations}
protected:
    virtual ~Inspector${domainName}BackendDispatcherHandler();
};""")

    BackendDispatcherHeaderDomainDispatcherDeclaration = (
    """${classAndExportMacro} Inspector${domainName}BackendDispatcher final : public Inspector::InspectorSupplementalBackendDispatcher {
public:
    static Ref<Inspector${domainName}BackendDispatcher> create(Inspector::InspectorBackendDispatcher*, Inspector${domainName}BackendDispatcherHandler*);
    virtual void dispatch(long callId, const String& method, Ref<Inspector::InspectorObject>&& message) override;
${commandDeclarations}
private:
    Inspector${domainName}BackendDispatcher(Inspector::InspectorBackendDispatcher&, Inspector${domainName}BackendDispatcherHandler*);
    Inspector${domainName}BackendDispatcherHandler* m_agent;
#if ENABLE(INSPECTOR_ALTERNATE_DISPATCHERS)
public:
    void setAlternateDispatcher(AlternateInspector${domainName}BackendDispatcher* alternateDispatcher) { m_alternateDispatcher = alternateDispatcher; }
private:
    AlternateInspector${domainName}BackendDispatcher* m_alternateDispatcher;
#endif
};""")

    BackendDispatcherHeaderAsyncCommandDeclaration = (
    """    ${classAndExportMacro} ${callbackName} : public Inspector::InspectorBackendDispatcher::CallbackBase {
    public:
        ${callbackName}(Ref<Inspector::InspectorBackendDispatcher>&&, int id);
        void sendSuccess(${outParameters});
    };
    virtual void ${commandName}(${inParameters}) = 0;""")

    BackendDispatcherImplementationSmallSwitch = (
    """void Inspector${domainName}BackendDispatcher::dispatch(long callId, const String& method, Ref<InspectorObject>&& message)
{
    Ref<Inspector${domainName}BackendDispatcher> protect(*this);

${dispatchCases}
    else
        m_backendDispatcher->reportProtocolError(&callId, InspectorBackendDispatcher::MethodNotFound, makeString('\\'', "${domainName}", '.', method, "' was not found"));
}""")

    BackendDispatcherImplementationLargeSwitch = (
"""void Inspector${domainName}BackendDispatcher::dispatch(long callId, const String& method, Ref<InspectorObject>&& message)
{
    Ref<Inspector${domainName}BackendDispatcher> protect(*this);

    typedef void (Inspector${domainName}BackendDispatcher::*CallHandler)(long callId, const Inspector::InspectorObject& message);
    typedef HashMap<String, CallHandler> DispatchMap;
    DEPRECATED_DEFINE_STATIC_LOCAL(DispatchMap, dispatchMap, ());
    if (dispatchMap.isEmpty()) {
        static const struct MethodTable {
            const char* name;
            CallHandler handler;
        } commands[] = {
${dispatchCases}
        };
        size_t length = WTF_ARRAY_LENGTH(commands);
        for (size_t i = 0; i < length; ++i)
            dispatchMap.add(commands[i].name, commands[i].handler);
    }

    HashMap<String, CallHandler>::iterator it = dispatchMap.find(method);
    if (it == dispatchMap.end()) {
        m_backendDispatcher->reportProtocolError(&callId, InspectorBackendDispatcher::MethodNotFound, makeString('\\'', "${domainName}", '.', method, "' was not found"));
        return;
    }

    ((*this).*it->value)(callId, message.get());
}""")

    BackendDispatcherImplementationDomainConstructor = (
    """Ref<Inspector${domainName}BackendDispatcher> Inspector${domainName}BackendDispatcher::create(InspectorBackendDispatcher* backendDispatcher, Inspector${domainName}BackendDispatcherHandler* agent)
{
    return adoptRef(*new Inspector${domainName}BackendDispatcher(*backendDispatcher, agent));
}

Inspector${domainName}BackendDispatcher::Inspector${domainName}BackendDispatcher(InspectorBackendDispatcher& backendDispatcher, Inspector${domainName}BackendDispatcherHandler* agent)
    : InspectorSupplementalBackendDispatcher(backendDispatcher)
    , m_agent(agent)
#if ENABLE(INSPECTOR_ALTERNATE_DISPATCHERS)
    , m_alternateDispatcher(nullptr)
#endif
{
    m_backendDispatcher->registerDispatcherForDomain(ASCIILiteral("${domainName}"), this);
}""")

    BackendDispatcherImplementationPrepareCommandArguments = (
"""    auto protocolErrors = Inspector::Protocol::Array<String>::create();
    RefPtr<InspectorObject> paramsContainer;
    message.getObject(ASCIILiteral("params"), paramsContainer);
${inParameterDeclarations}
    if (protocolErrors->length()) {
        String errorMessage = String::format("Some arguments of method \'%s\' can't be processed", "${domainName}.${commandName}");
        m_backendDispatcher->reportProtocolError(&callId, InspectorBackendDispatcher::InvalidParams, errorMessage, WTF::move(protocolErrors));
        return;
    }
""")

    BackendDispatcherImplementationAsyncCommand = (
"""Inspector${domainName}BackendDispatcherHandler::${callbackName}::${callbackName}(Ref<InspectorBackendDispatcher>&& backendDispatcher, int id) : Inspector::InspectorBackendDispatcher::CallbackBase(WTF::move(backendDispatcher), id) { }

void Inspector${domainName}BackendDispatcherHandler::${callbackName}::sendSuccess(${formalParameters})
{
    Ref<InspectorObject> jsonMessage = InspectorObject::create();
${outParameterAssignments}
    sendIfActive(WTF::move(jsonMessage), ErrorString());
}""")

    FrontendDispatcherDomainDispatcherDeclaration = (
"""${classAndExportMacro} Inspector${domainName}FrontendDispatcher {
public:
    Inspector${domainName}FrontendDispatcher(InspectorFrontendChannel* inspectorFrontendChannel) : m_inspectorFrontendChannel(inspectorFrontendChannel) { }
${eventDeclarations}
private:
    InspectorFrontendChannel* m_inspectorFrontendChannel;
};""")

    ProtocolObjectBuilderDeclarationPrelude = (
"""    template<int STATE>
    class Builder {
    private:
        RefPtr<Inspector::InspectorObject> m_result;

        template<int STEP> Builder<STATE | STEP>& castState()
        {
            return *reinterpret_cast<Builder<STATE | STEP>*>(this);
        }

        Builder(Ref</*${objectType}*/Inspector::InspectorObject>&& object)
            : m_result(WTF::move(object))
        {
            COMPILE_ASSERT(STATE == NoFieldsSet, builder_created_in_non_init_state);
        }
        friend class ${objectType};
    public:""")

    ProtocolObjectBuilderDeclarationPostlude = (
"""
        Ref<${objectType}> release()
        {
            COMPILE_ASSERT(STATE == AllFieldsSet, result_is_not_ready);
            COMPILE_ASSERT(sizeof(${objectType}) == sizeof(Inspector::InspectorObject), cannot_cast);

            Ref<Inspector::InspectorObject> result = m_result.releaseNonNull();
            return WTF::move(*reinterpret_cast<Ref<${objectType}>*>(&result));
        }
    };

    /*
     * Synthetic constructor:
${constructorExample}
     */
    static Builder<NoFieldsSet> create()
    {
        return Builder<NoFieldsSet>(Inspector::InspectorObject::create());
    }""")

    ProtocolObjectRuntimeCast = (
"""RefPtr<${objectType}> BindingTraits<${objectType}>::runtimeCast(RefPtr<Inspector::InspectorValue>&& value)
{
    RefPtr<Inspector::InspectorObject> result;
    bool castSucceeded = value->asObject(result);
    ASSERT_UNUSED(castSucceeded, castSucceeded);
#if !ASSERT_DISABLED
    BindingTraits<${objectType}>::assertValueHasExpectedType(result.get());
#endif  // !ASSERT_DISABLED
    COMPILE_ASSERT(sizeof(${objectType}) == sizeof(Inspector::InspectorObjectBase), type_cast_problem);
    return static_cast<${objectType}*>(static_cast<Inspector::InspectorObjectBase*>(result.get()));
}
""")
