#!/usr/bin/env python
#
# Copyright (c) 2014-2016 Apple Inc. All rights reserved.
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

try:
    from .cpp_generator import CppGenerator
    from .cpp_generator_templates import CppGeneratorTemplates as CppTemplates
    from .generator import Generator, ucfirst
    from .models import ObjectType, ArrayType
except:
    from cpp_generator import CppGenerator
    from cpp_generator_templates import CppGeneratorTemplates as CppTemplates
    from generator import Generator, ucfirst
    from models import ObjectType, ArrayType

log = logging.getLogger('global')


class CppFrontendDispatcherImplementationGenerator(CppGenerator):
    def __init__(self, *args, **kwargs):
        CppGenerator.__init__(self, *args, **kwargs)

    def output_filename(self):
        return "%sFrontendDispatchers.cpp" % self.protocol_name()

    def domains_to_generate(self):
        return [domain for domain in Generator.domains_to_generate(self) if len(self.events_for_domain(domain)) > 0]

    def generate_output(self):
        header_args = {
            'primaryInclude': '"%sFrontendDispatchers.h"' % self.protocol_name(),
            'secondaryIncludes': self._generate_secondary_header_includes(),
        }

        sections = []
        sections.append(self.generate_license())
        sections.append(Template(CppTemplates.ImplementationPrelude).substitute(None, **header_args))
        sections.extend(list(map(self._generate_dispatcher_implementations_for_domain, self.domains_to_generate())))
        sections.append(Template(CppTemplates.ImplementationPostlude).substitute(None, **header_args))
        return "\n\n".join(sections)

    # Private methods.

    def _generate_secondary_header_includes(self):
        header_includes = [
            (["JavaScriptCore", "WebKit"], ("JavaScriptCore", "inspector/InspectorFrontendRouter.h")),
            (["JavaScriptCore", "WebKit"], ("WTF", "wtf/text/CString.h"))
        ]

        return '\n'.join(self.generate_includes_from_entries(header_includes))

    def _generate_dispatcher_implementations_for_domain(self, domain):
        implementations = []
        events = self.events_for_domain(domain)
        for event in events:
            implementations.append(self._generate_dispatcher_implementation_for_event(event, domain))

        return self.wrap_with_guard_for_condition(domain.condition, '\n\n'.join(implementations))

    def _generate_dispatcher_implementation_for_event(self, event, domain):
        lines = []
        parameter_assignments = []
        formal_parameters = []

        for parameter in event.event_parameters:

            parameter_value = parameter.parameter_name
            if parameter.is_optional and not CppGenerator.should_pass_by_copy_for_return_type(parameter.type):
                parameter_value = '*' + parameter_value
            if parameter.type.is_enum():
                parameter_value = 'Inspector::Protocol::%s::getEnumConstantValue(%s)' % (self.helpers_namespace(), parameter_value)

            parameter_args = {
                'parameterType': CppGenerator.cpp_type_for_stack_out_parameter(parameter),
                'parameterName': parameter.parameter_name,
                'parameterValue': parameter_value,
                'keyedSetMethod': CppGenerator.cpp_setter_method_for_type(parameter.type),
            }

            if parameter.is_optional:
                parameter_assignments.append('    if (%(parameterName)s)' % parameter_args)
                parameter_assignments.append('        paramsObject->%(keyedSetMethod)s("%(parameterName)s"_s, %(parameterValue)s);' % parameter_args)
            else:
                parameter_assignments.append('    paramsObject->%(keyedSetMethod)s("%(parameterName)s"_s, %(parameterValue)s);' % parameter_args)

            formal_parameters.append('%s %s' % (CppGenerator.cpp_type_for_checked_formal_event_parameter(parameter), parameter.parameter_name))

        event_args = {
            'domainName': domain.domain_name,
            'eventName': event.event_name,
            'formalParameters': ", ".join(formal_parameters)
        }

        lines.append('void %(domainName)sFrontendDispatcher::%(eventName)s(%(formalParameters)s)' % event_args)
        lines.append('{')
        lines.append('    Ref<JSON::Object> jsonMessage = JSON::Object::create();')
        lines.append('    jsonMessage->setString("method"_s, "%(domainName)s.%(eventName)s"_s);' % event_args)

        if len(parameter_assignments) > 0:
            lines.append('    Ref<JSON::Object> paramsObject = JSON::Object::create();')
            lines.extend(parameter_assignments)
            lines.append('    jsonMessage->setObject("params"_s, WTFMove(paramsObject));')

        lines.append('')
        lines.append('    m_frontendRouter.sendEvent(jsonMessage->toJSONString());')
        lines.append('}')
        return self.wrap_with_guard_for_condition(event.condition, "\n".join(lines))
