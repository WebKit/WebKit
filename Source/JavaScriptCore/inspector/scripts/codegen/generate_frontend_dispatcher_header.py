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

from models import EnumType
from generator import Generator, ucfirst
from generator_templates import GeneratorTemplates as Templates

log = logging.getLogger('global')


class FrontendDispatcherHeaderGenerator(Generator):
    def __init__(self, model, input_filepath):
        Generator.__init__(self, model, input_filepath)

    def output_filename(self):
        return "Inspector%sFrontendDispatchers.h" % self.model().framework.setting('prefix')

    def domains_to_generate(self):
        return filter(lambda domain: len(domain.events) > 0, Generator.domains_to_generate(self))

    def generate_output(self):
        headers = [
            '"Inspector%sProtocolTypes.h"' % self.model().framework.setting('prefix'),
            '<inspector/InspectorFrontendChannel.h>',
            '<inspector/InspectorValues.h>',
            '<wtf/PassRefPtr.h>',
            '<wtf/text/WTFString.h>']

        header_args = {
            'headerGuardString': re.sub('\W+', '_', self.output_filename()),
            'includes': '\n'.join(['#include ' + header for header in headers]),
            'typedefs': '',
            'inputFilename': self._input_filepath
        }

        sections = []
        sections.append(self.generate_license())
        sections.append(Template(Templates.CppHeaderPrelude).substitute(None, **header_args))
        sections.extend(map(self._generate_dispatcher_declarations_for_domain, self.domains_to_generate()))
        sections.append(Template(Templates.CppHeaderPostlude).substitute(None, **header_args))
        return "\n\n".join(sections)

    # Private methods.

    def _generate_anonymous_enum_for_parameter(self, parameter, event):
        enum_args = {
            'parameterName': parameter.parameter_name,
            'eventName': event.event_name
        }

        lines = []
        lines.append('        // Named after parameter \'%(parameterName)s\' while generating command/event %(eventName)s.' % enum_args)
        lines.append('        enum class %s {' % ucfirst(parameter.parameter_name))
        for enum_value in parameter.type.enum_values():
            lines.append('            %s = %d,' % (Generator.stylized_name_for_enum_value(enum_value), self.encoding_for_enum_value(enum_value)))
        lines.append('        }; // enum class %s' % ucfirst(parameter.parameter_name))
        return "\n".join(lines)

    def _generate_dispatcher_declarations_for_domain(self, domain):
        classComponents = ['class']
        exportMacro = self.model().framework.setting('export_macro', None)
        if exportMacro is not None:
            classComponents.append(exportMacro)

        used_enum_names = set([])

        event_declarations = []
        for event in domain.events:
            event_declarations.append(self._generate_dispatcher_declaration_for_event(event, domain, used_enum_names))

        handler_args = {
            'classAndExportMacro': " ".join(classComponents),
            'domainName': domain.domain_name,
            'eventDeclarations': "\n".join(event_declarations)
        }

        return Generator.wrap_with_guard_for_domain(domain, Template(Templates.FrontendDispatcherDomainDispatcherDeclaration).substitute(None, **handler_args))

    def _generate_dispatcher_declaration_for_event(self, event, domain, used_enum_names):
        formal_parameters = []
        lines = []
        for parameter in event.event_parameters:
            formal_parameters.append('%s %s' % (Generator.type_string_for_checked_formal_event_parameter(parameter), parameter.parameter_name))
            if isinstance(parameter.type, EnumType) and parameter.parameter_name not in used_enum_names:
                lines.append(self._generate_anonymous_enum_for_parameter(parameter, event))
                used_enum_names.add(parameter.parameter_name)

        lines.append("    void %s(%s);" % (event.event_name, ", ".join(formal_parameters)))
        return "\n".join(lines)
