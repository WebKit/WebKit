#!/usr/bin/env python
#
# Copyright (c) 2014, 2015 Apple Inc. All rights reserved.
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

from builtins_generator import BuiltinsGenerator
from builtins_templates import BuiltinsGeneratorTemplates as Templates

log = logging.getLogger('global')


class BuiltinsSeparateWrapperGenerator(BuiltinsGenerator):
    def __init__(self, model, object):
        BuiltinsGenerator.__init__(self, model)
        self.object = object

    def output_filename(self):
        return "%sBuiltinsWrapper.h" % BuiltinsGenerator.mangledNameForObject(self.object)

    def macro_prefix(self):
        return self.model().framework.setting('macro_prefix')

    def generate_output(self):
        args = {
            'namespace': self.model().framework.setting('namespace'),
            'headerGuard': self.output_filename().replace('.', '_'),
            'macroPrefix': self.macro_prefix(),
            'objectName': self.object.object_name,
            'objectMacro': self.object.object_name.upper(),
        }

        sections = []
        sections.append(self.generate_license())
        sections.append(Template(Templates.DoNotEditWarning).substitute(args))
        sections.append(Template(Templates.HeaderIncludeGuardTop).substitute(args))
        sections.append(self.generate_header_includes())
        sections.append(Template(Templates.NamespaceTop).substitute(args))
        sections.append(Template(Templates.SeparateWrapperHeaderBoilerplate).substitute(args))
        sections.append(Template(Templates.NamespaceBottom).substitute(args))
        sections.append(Template(Templates.HeaderIncludeGuardBottom).substitute(args))

        return "\n\n".join(sections)

    def generate_header_includes(self):
        header_includes = [
            (["WebCore"],
                ("WebCore", "%sBuiltins.h" % self.object.object_name),
            ),

            (["WebCore"],
                ("JavaScriptCore", "bytecode/UnlinkedFunctionExecutable.h"),
            ),

            (["WebCore"],
                ("JavaScriptCore", "builtins/BuiltinUtils.h"),
            ),

            (["WebCore"],
                ("JavaScriptCore", "runtime/Identifier.h"),
            ),

            (["WebCore"],
                ("JavaScriptCore", "runtime/JSFunction.h"),
            ),
        ]

        return '\n'.join(self.generate_includes_from_entries(header_includes))
