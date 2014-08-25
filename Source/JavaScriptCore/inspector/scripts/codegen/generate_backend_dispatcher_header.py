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
import re
import string
from string import Template

from generator import Generator, ucfirst
from generator_templates import GeneratorTemplates as Templates
from models import EnumType

log = logging.getLogger('global')


class BackendDispatcherHeaderGenerator(Generator):
    def __init__(self, model, input_filepath):
        Generator.__init__(self, model, input_filepath)

    def output_filename(self):
        return "Inspector%sBackendDispatchers.h" % self.model().framework.setting('prefix')

    def domains_to_generate(self):
        return filter(lambda domain: len(domain.commands) > 0, Generator.domains_to_generate(self))

    def generate_output(self):
        headers = [
            '"Inspector%sProtocolTypes.h"' % self.model().framework.setting('prefix'),
            '<inspector/InspectorBackendDispatcher.h>',
            '<wtf/PassRefPtr.h>',
            '<wtf/text/WTFString.h>']

        typedefs = [('String', 'ErrorString')]

        header_args = {
            'headerGuardString': re.sub('\W+', '_', self.output_filename()),
            'includes': '\n'.join(['#include ' + header for header in headers]),
            'typedefs': '\n'.join(['typedef %s %s;' % typedef for typedef in typedefs]),
            'inputFilename': self._input_filepath
        }

        sections = []
        sections.append(self.generate_license())
        sections.append(Template(Templates.CppHeaderPrelude).substitute(None, **header_args))
        sections.extend(map(self._generate_handler_declarations_for_domain, self.domains_to_generate()))
        sections.extend(map(self._generate_dispatcher_declarations_for_domain, self.domains_to_generate()))
        sections.append(Template(Templates.CppHeaderPostlude).substitute(None, **header_args))
        return "\n\n".join(sections)

    # Private methods.

    def _generate_handler_declarations_for_domain(self, domain):
        classComponents = ['class']
        exportMacro = self.model().framework.setting('export_macro', None)
        if exportMacro is not None:
            classComponents.append(exportMacro)

        used_enum_names = set()

        command_declarations = []
        for command in domain.commands:
            command_declarations.append(self._generate_handler_declaration_for_command(command, used_enum_names))

        handler_args = {
            'classAndExportMacro': " ".join(classComponents),
            'domainName': domain.domain_name,
            'commandDeclarations': "\n".join(command_declarations)
        }

        return Generator.wrap_with_guard_for_domain(domain, Template(Templates.BackendDispatcherHeaderDomainHandlerDeclaration).substitute(None, **handler_args))

    def _generate_anonymous_enum_for_parameter(self, parameter, command):
        enum_args = {
            'parameterName': parameter.parameter_name,
            'commandName': command.command_name
        }

        lines = []
        lines.append('    // Named after parameter \'%(parameterName)s\' while generating command/event %(commandName)s.' % enum_args)
        lines.append('    enum class %s {' % ucfirst(parameter.parameter_name))
        for enum_value in parameter.type.enum_values():
            lines.append('        %s = %d,' % (Generator.stylized_name_for_enum_value(enum_value), self.encoding_for_enum_value(enum_value)))
        lines.append('    }; // enum class %s' % ucfirst(parameter.parameter_name))
        return '\n'.join(lines)

    def _generate_handler_declaration_for_command(self, command, used_enum_names):
        if command.is_async:
            return self._generate_async_handler_declaration_for_command(command)

        lines = []
        parameters = ['ErrorString*']
        for _parameter in command.call_parameters:
            parameters.append("%s in_%s" % (Generator.type_string_for_unchecked_formal_in_parameter(_parameter), _parameter.parameter_name))

            if isinstance(_parameter.type, EnumType) and _parameter.parameter_name not in used_enum_names:
                lines.append(self._generate_anonymous_enum_for_parameter(_parameter, command))
                used_enum_names.add(_parameter.parameter_name)

        for _parameter in command.return_parameters:
            parameter_name = 'out_' + _parameter.parameter_name
            if _parameter.is_optional:
                parameter_name = 'opt_' + parameter_name
            parameters.append("%s %s" % (Generator.type_string_for_formal_out_parameter(_parameter), parameter_name))

            if isinstance(_parameter.type, EnumType) and _parameter.parameter_name not in used_enum_names:
                lines.append(self._generate_anonymous_enum_for_parameter(_parameter, command))
                used_enum_names.add(_parameter.parameter_name)

        command_args = {
            'commandName': command.command_name,
            'parameters': ", ".join(parameters)
        }
        lines.append('    virtual void %(commandName)s(%(parameters)s) = 0;' % command_args)
        return '\n'.join(lines)

    def _generate_async_handler_declaration_for_command(self, command):
        callbackName = "%sCallback" % ucfirst(command.command_name)

        in_parameters = ['ErrorString*']
        for _parameter in command.call_parameters:
            in_parameters.append("%s in_%s" % (Generator.type_string_for_unchecked_formal_in_parameter(_parameter), _parameter.parameter_name))
        in_parameters.append("PassRefPtr<%s> callback" % callbackName)

        out_parameters = []
        for _parameter in command.return_parameters:
            out_parameters.append("%s %s" % (Generator.type_string_for_formal_async_parameter(_parameter), _parameter.parameter_name))

        command_args = {
            'callbackName': callbackName,
            'commandName': command.command_name,
            'inParameters': ", ".join(in_parameters),
            'outParameters': ", ".join(out_parameters),
        }

        return Template(Templates.BackendDispatcherHeaderAsyncCommandDeclaration).substitute(None, **command_args)

    def _generate_dispatcher_declarations_for_domain(self, domain):
        classComponents = ['class']
        exportMacro = self.model().framework.setting('export_macro', None)
        if exportMacro is not None:
            classComponents.append(exportMacro)

        declarations = []
        if len(domain.commands) > 0:
            declarations.append('private:')
        declarations.extend(map(self._generate_dispatcher_declaration_for_command, domain.commands))

        handler_args = {
            'classAndExportMacro': " ".join(classComponents),
            'domainName': domain.domain_name,
            'commandDeclarations': "\n".join(declarations)
        }

        return Generator.wrap_with_guard_for_domain(domain, Template(Templates.BackendDispatcherHeaderDomainDispatcherDeclaration).substitute(None, **handler_args))

    def _generate_dispatcher_declaration_for_command(self, command):
        return "    void %s(long callId, const Inspector::InspectorObject& message);" % command.command_name
