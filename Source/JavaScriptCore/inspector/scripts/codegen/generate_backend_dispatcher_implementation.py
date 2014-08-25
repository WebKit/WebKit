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


import logging
import string
from string import Template

from generator import Generator, ucfirst
from generator_templates import GeneratorTemplates as Templates
from models import ObjectType, ArrayType

log = logging.getLogger('global')


class BackendDispatcherImplementationGenerator(Generator):
    def __init__(self, model, input_filepath):
        Generator.__init__(self, model, input_filepath)

    def output_filename(self):
        return "Inspector%sBackendDispatchers.cpp" % self.model().framework.setting('prefix')

    def domains_to_generate(self):
        return filter(lambda domain: len(domain.commands) > 0, Generator.domains_to_generate(self))

    def generate_output(self):
        secondary_headers = [
            '<inspector/InspectorFrontendChannel.h>',
            '<inspector/InspectorValues.h>',
            '<wtf/text/CString.h>']

        header_args = {
            'primaryInclude': '"Inspector%sBackendDispatchers.h"' % self.model().framework.setting('prefix'),
            'secondaryIncludes': "\n".join(['#include %s' % header for header in secondary_headers]),
            'inputFilename': self._input_filepath
        }

        sections = []
        sections.append(self.generate_license())
        sections.append(Template(Templates.CppImplementationPrelude).substitute(None, **header_args))
        sections.append("\n".join(map(self._generate_handler_class_destructor_for_domain, self.domains_to_generate())))
        sections.extend(map(self._generate_dispatcher_implementations_for_domain, self.domains_to_generate()))
        sections.append(Template(Templates.CppImplementationPostlude).substitute(None, **header_args))
        return "\n\n".join(sections)

    # Private methods.

    def _generate_handler_class_destructor_for_domain(self, domain):
        destructor_args = {
            'domainName': domain.domain_name
        }
        destructor = 'Inspector%(domainName)sBackendDispatcherHandler::~Inspector%(domainName)sBackendDispatcherHandler() { }' % destructor_args
        return Generator.wrap_with_guard_for_domain(domain, destructor)

    def _generate_dispatcher_implementations_for_domain(self, domain):
        implementations = []

        constructor_args = {
            'domainName': domain.domain_name,
        }
        implementations.append(Template(Templates.BackendDispatcherImplementationDomainConstructor).substitute(None, **constructor_args))

        if len(domain.commands) <= 5:
            implementations.append(self._generate_small_dispatcher_switch_implementation_for_domain(domain))
        else:
            implementations.append(self._generate_large_dispatcher_switch_implementation_for_domain(domain))

        for command in domain.commands:
            if command.is_async:
                implementations.append(self._generate_async_dispatcher_class_for_domain(command, domain))
            implementations.append(self._generate_dispatcher_implementation_for_command(command, domain))

        return Generator.wrap_with_guard_for_domain(domain, "\n\n".join(implementations))

    def _generate_small_dispatcher_switch_implementation_for_domain(self, domain):
        cases = []
        cases.append('    if (method == "%s")' % domain.commands[0].command_name)
        cases.append('        %s(callId, *message.get());' % domain.commands[0].command_name)
        for command in domain.commands[1:]:
            cases.append('    else if (method == "%s")' % command.command_name)
            cases.append('        %s(callId, *message.get());' % command.command_name)

        switch_args = {
            'domainName': domain.domain_name,
            'dispatchCases': "\n".join(cases)
        }

        return Template(Templates.BackendDispatcherImplementationSmallSwitch).substitute(None, **switch_args)

    def _generate_large_dispatcher_switch_implementation_for_domain(self, domain):
        cases = []
        for command in domain.commands:
            args = {
                'domainName': domain.domain_name,
                'commandName': command.command_name
            }
            cases.append('            { "%(commandName)s", &Inspector%(domainName)sBackendDispatcher::%(commandName)s },' % args)

        switch_args = {
            'domainName': domain.domain_name,
            'dispatchCases': "\n".join(cases)
        }

        return Template(Templates.BackendDispatcherImplementationLargeSwitch).substitute(None, **switch_args)

    def _generate_async_dispatcher_class_for_domain(self, command, domain):
        out_parameter_assignments = []
        formal_parameters = []

        for parameter in command.return_parameters:
            param_args = {
                'frameworkPrefix': self.model().framework.setting('prefix'),
                'keyedSetMethod': Generator.keyed_set_method_for_type(parameter.type),
                'parameterName': parameter.parameter_name,
                'parameterType': Generator.type_string_for_stack_in_parameter(parameter),
            }

            formal_parameters.append('%s %s' % (Generator.type_string_for_formal_async_parameter(parameter), parameter.parameter_name))

            if parameter.is_optional:
                if Generator.should_use_wrapper_for_return_type(parameter.type):
                    out_parameter_assignments.append('    if (%(parameterName)s.isAssigned())' % param_args)
                    out_parameter_assignments.append('        jsonMessage->%(keyedSetMethod)s(ASCIILiteral("%(parameterName)s"), %(parameterName)s.getValue());' % param_args)
                else:
                    out_parameter_assignments.append('    if (%(parameterName)s)' % param_args)
                    out_parameter_assignments.append('        jsonMessage->%(keyedSetMethod)s(ASCIILiteral("%(parameterName)s"), %(parameterName)s);' % param_args)
            elif parameter.type.is_enum():
                out_parameter_assignments.append('    jsonMessage->%(keyedSetMethod)s(ASCIILiteral("%(parameterName)s"), Inspector::Protocol::get%(frameworkPrefix)sEnumConstantValue(%(parameterName)s));' % param_args)
            else:
                out_parameter_assignments.append('    jsonMessage->%(keyedSetMethod)s(ASCIILiteral("%(parameterName)s"), %(parameterName)s);' % param_args)

        async_args = {
            'domainName': domain.domain_name,
            'callbackName': ucfirst(command.command_name) + 'Callback',
            'formalParameters': ", ".join(formal_parameters),
            'outParameterAssignments': "\n".join(out_parameter_assignments)
        }
        return Template(Templates.BackendDispatcherImplementationAsyncCommand).substitute(None, **async_args)

    def _generate_dispatcher_implementation_for_command(self, command, domain):
        in_parameter_declarations = []
        out_parameter_declarations = []
        out_parameter_assignments = []
        method_parameters = ['&error']

        for parameter in command.call_parameters:
            out_success_argument = 'nullptr'
            if parameter.is_optional:
                out_success_argument = '&%s_valueFound' % parameter.parameter_name
                in_parameter_declarations.append('    bool %s_valueFound = false;' % parameter.parameter_name)

            param_args = {
                'parameterType': Generator.type_string_for_stack_in_parameter(parameter),
                'parameterName': parameter.parameter_name,
                'keyedGetMethod': Generator.keyed_get_method_for_type(parameter.type),
                'successOutParam': out_success_argument
            }

            in_parameter_declarations.append('    %(parameterType)s in_%(parameterName)s = InspectorBackendDispatcher::%(keyedGetMethod)s(paramsContainerPtr, ASCIILiteral("%(parameterName)s"), %(successOutParam)s, protocolErrorsPtr);' % param_args)

            if parameter.is_optional:
                method_parameters.append('%(parameterName)s_valueFound ? &in_%(parameterName)s : nullptr' % param_args)
            else:
                method_parameters.append('in_' + parameter.parameter_name)

        if command.is_async:
            async_args = {
                'domainName': domain.domain_name,
                'callbackName': ucfirst(command.command_name) + 'Callback'
            }

            out_parameter_assignments.append('        callback->disable();')
            out_parameter_assignments.append('        m_backendDispatcher->reportProtocolError(&callId, Inspector::InspectorBackendDispatcher::ServerError, error);')
            out_parameter_assignments.append('        return;')
            method_parameters.append('callback')

        else:
            for parameter in command.return_parameters:
                param_args = {
                    'parameterType': Generator.type_string_for_stack_out_parameter(parameter),
                    'parameterName': parameter.parameter_name,
                    'keyedSetMethod': Generator.keyed_set_method_for_type(parameter.type),
                    'frameworkPrefix': self.model().framework.setting('prefix')
                }

                out_parameter_declarations.append('    %(parameterType)s out_%(parameterName)s;' % param_args)
                if parameter.is_optional:
                    if Generator.should_use_wrapper_for_return_type(parameter.type):
                        out_parameter_assignments.append('        if (out_%(parameterName)s.isAssigned())' % param_args)
                        out_parameter_assignments.append('            result->%(keyedSetMethod)s(ASCIILiteral("%(parameterName)s"), out_%(parameterName)s.getValue());' % param_args)
                    else:
                        out_parameter_assignments.append('        if (out_%(parameterName)s)' % param_args)
                        out_parameter_assignments.append('            result->%(keyedSetMethod)s(ASCIILiteral("%(parameterName)s"), out_%(parameterName)s);' % param_args)
                elif parameter.type.is_enum():
                    out_parameter_assignments.append('        result->%(keyedSetMethod)s(ASCIILiteral("%(parameterName)s"), Inspector::Protocol::get%(frameworkPrefix)sEnumConstantValue(out_%(parameterName)s));' % param_args)
                else:
                    out_parameter_assignments.append('        result->%(keyedSetMethod)s(ASCIILiteral("%(parameterName)s"), out_%(parameterName)s);' % param_args)

                if Generator.should_pass_by_copy_for_return_type(parameter.type):
                    method_parameters.append('out_' + parameter.parameter_name)
                else:
                    method_parameters.append('&out_' + parameter.parameter_name)

        command_args = {
            'domainName': domain.domain_name,
            'callbackName': '%sCallback' % ucfirst(command.command_name),
            'commandName': command.command_name,
            'inParameterDeclarations': "\n".join(in_parameter_declarations),
            'invocationParameters': ", ".join(method_parameters)
        }

        lines = []
        if len(command.call_parameters) == 0:
            lines.append('void Inspector%(domainName)sBackendDispatcher::%(commandName)s(long callId, const InspectorObject&)' % command_args)
        else:
            lines.append('void Inspector%(domainName)sBackendDispatcher::%(commandName)s(long callId, const InspectorObject& message)' % command_args)
        lines.append('{')

        if len(command.call_parameters) > 0:
            lines.append(Template(Templates.BackendDispatcherImplementationPrepareCommandArguments).substitute(None, **command_args))

        lines.append('    ErrorString error;')
        lines.append('    RefPtr<InspectorObject> result = InspectorObject::create();')
        if command.is_async:
            lines.append('    RefPtr<Inspector%(domainName)sBackendDispatcherHandler::%(callbackName)s> callback = adoptRef(new Inspector%(domainName)sBackendDispatcherHandler::%(callbackName)s(m_backendDispatcher,callId));' % command_args)
        if len(command.return_parameters) > 0:
            lines.extend(out_parameter_declarations)
        lines.append('    m_agent->%(commandName)s(%(invocationParameters)s);' % command_args)
        lines.append('')
        if command.is_async:
            lines.append('    if (error.length()) {')
            lines.extend(out_parameter_assignments)
            lines.append('    }')
        elif len(command.return_parameters) > 1:
            lines.append('    if (!error.length()) {')
            lines.extend(out_parameter_assignments)
            lines.append('    }')
        elif len(command.return_parameters) == 1:
            lines.append('    if (!error.length())')
            lines.extend(out_parameter_assignments)
            lines.append('')

        if not command.is_async:
            lines.append('    m_backendDispatcher->sendResponse(callId, result.release(), error);')
        lines.append('}')
        return "\n".join(lines)
