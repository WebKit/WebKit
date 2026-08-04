#!/usr/bin/env python3
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
import os.path
import re
from string import Template
import json

from builtins_model import BuiltinFunction, BuiltinObject
from builtins_templates import BuiltinsGeneratorTemplates as Templates

log = logging.getLogger('global')

# These match WK_lcfirst and WK_ucfirst defined in CodeGenerator.pm.
def WK_lcfirst(str):
    str = str[:1].lower() + str[1:]
    str = str.replace('dOM', 'dom')
    str = str.replace('uRL', 'url')
    str = str.replace('jS', 'js')
    str = str.replace('xML', 'xml')
    str = str.replace('xSLT', 'xslt')
    str = str.replace('cSS', 'css')
    str = str.replace('rTC', 'rtc')
    return str

def WK_ucfirst(str):
    str = str[:1].upper() + str[1:]
    str = str.replace('Xml', 'XML')
    str = str.replace('Svg', 'SVG')
    return str


# Matches Lexer<Latin1Character>::isWhiteSpace, which excludes line terminators.
def is_js_white_space(ch):
    return ch in (' ', '\t', '\v', '\f', '\xa0')


# Transliterated from computeBuiltinSourceMetadata, which still scans at runtime
# for sources that have no generated entry. Change one and you must change the
# other; keeping the two statement-for-statement alike is what makes them agree.
def compute_builtin_source_metadata(characters):
    regular_function_begin = "(function ("
    async_function_begin = "(async function ("
    assert len(characters) >= len("(function (){})")
    is_async_function = len(characters) >= len("(async function (){})") and characters.startswith(async_function_begin)
    assert is_async_function or characters.startswith(regular_function_begin)

    async_offset = len("async ") if is_async_function else 0
    parameters_start = len("function (") + async_offset
    is_in_strict_context = False

    i = parameters_start + 1
    commas = 0
    inside_curly_brackets = False
    saw_one_param = False
    has_rest_param = False
    while True:
        assert i < len(characters)
        if characters[i] == ')':
            break

        if characters[i] == '}':
            inside_curly_brackets = False
        elif characters[i] == '{' or inside_curly_brackets:
            inside_curly_brackets = True
            i += 1
            continue
        elif characters[i] == ',':
            commas += 1
        elif not is_js_white_space(characters[i]):
            saw_one_param = True

        if i + 2 < len(characters) and characters[i:i + 3] == '...':
            has_rest_param = True
            i += 2

        i += 1

    if commas:
        parameter_count = commas + 1
    elif saw_one_param:
        parameter_count = 1
    else:
        parameter_count = 0

    if has_rest_param:
        assert parameter_count
        parameter_count -= 1

    line_count = 0
    end_column = 0
    offset_of_last_newline = 0
    offset_of_second_to_last_newline = None
    use_strict = "use strict"
    i = 0
    while i < len(characters):
        if characters[i] == '\n':
            if line_count:
                offset_of_second_to_last_newline = offset_of_last_newline
            line_count += 1
            end_column = 0
            offset_of_last_newline = i
        else:
            end_column += 1

        if not is_in_strict_context and characters[i] in ('"', '\''):
            if i + 1 + len(use_strict) < len(characters) and characters.startswith(use_strict, i + 1):
                is_in_strict_context = True
                i += 1 + len(use_strict)

        i += 1

    position_before_last_newline_line_start_offset = offset_of_second_to_last_newline + 1 if offset_of_second_to_last_newline is not None else 0

    close_brace_offset_from_end = 1
    while characters[len(characters) - close_brace_offset_from_end] != '}':
        close_brace_offset_from_end += 1

    return {
        'sourceLength': len(characters),
        'parametersStart': parameters_start,
        'parameterCount': parameter_count,
        'lineCount': line_count,
        'endColumn': end_column,
        'offsetOfLastNewline': offset_of_last_newline,
        'positionBeforeLastNewlineLineStartOffset': position_before_last_newline_line_start_offset,
        'closeBraceOffsetFromEnd': close_brace_offset_from_end,
        'isAsyncFunction': 'true' if is_async_function else 'false',
        'isInStrictContext': 'true' if is_in_strict_context else 'false',
    }


class BuiltinsGenerator:
    def __init__(self, model):
        self._model = model

    def model(self):
        return self._model

    # These methods are overridden by subclasses.

    def generate_output(self):
        pass

    def output_filename(self):
        pass


    # Shared code generation methods.
    def generate_license(self):
        raw_license = Template(Templates.LicenseText).substitute(None)
        copyrights = self._model.copyrights()
        copyrights.sort()

        license_block = []
        license_block.append("/*")
        for copyright in copyrights:
            license_block.append(" * Copyright (c) %s" % copyright)
        if len(copyrights) > 0:
            license_block.append(" * ")

        for line in raw_license.split('\n'):
            license_block.append(" * " + line)

        license_block.append(" */")

        return '\n'.join(license_block)

    def generate_includes_from_entries(self, entries):
        includes = set()
        for entry in entries:
            (allowed_framework_names, data) = entry
            (framework_name, header_path) = data

            if self.model().framework.name not in allowed_framework_names:
                continue
            if self.model().framework.name != framework_name:
                includes.add("#include <%s/%s>" % (framework_name, os.path.basename(header_path)))
            else:
                includes.add("#include \"%s\"" % os.path.basename(header_path))

        return sorted(list(includes))

    def generate_primary_header_includes(self):
        name, _ = os.path.splitext(self.output_filename())
        return '\n'.join([
            "#include \"config.h\"",
            "#include \"%s.h\"" % name,
        ])

    def generate_embedded_code_data_for_function(self, function):
        text = function.function_source
        # Wrap it in parens to avoid adding to global scope.
        function_type_string = "function "
        if function.is_async:
            function_type_string = "async " + function_type_string

        text = "(" + function_type_string + text[text.index("("):] + ")"
        embeddedSourceLength = len(text) + 1  # For extra \n.
        # Lazy way to escape quotes, I think?
        textLines = json.dumps(text)[1:-1].split("\\n")
        # This looks scary because we need the JS source itself to have newlines.
        embeddedSource = '\n'.join(['    "%s\\n" \\' % line for line in textLines])

        constructAbility = "CannotConstruct"
        if function.is_constructor:
            constructAbility = "CanConstruct"

        constructorKind = "None"
        if function.is_naked_constructor:
            constructorKind = "Naked"

        inlineAttribute = "None"
        if function.is_always_inline:
            inlineAttribute = "Always"

        return {
            'codeName': BuiltinsGenerator.mangledNameForFunction(function) + 'Code',
            'embeddedSource': embeddedSource,
            'embeddedSourceLength': embeddedSourceLength,
            'originalSource': text + "\n",
            'constructAbility': constructAbility,
            'constructorKind': constructorKind,
            'inlineAttribute': inlineAttribute,
            'visibility': function.visibility,
            'intrinsic': function.intrinsic
        }

    def generate_embedded_code_string_section_for_data(self, data):
        lines = []
        lines.append("constinit const JSC::ConstructAbility s_%(codeName)sConstructAbility = JSC::ConstructAbility::%(constructAbility)s;" % data)
        lines.append("constinit const JSC::ConstructorKind s_%(codeName)sConstructorKind = JSC::ConstructorKind::%(constructorKind)s;" % data)
        lines.append("constinit const JSC::ImplementationVisibility s_%(codeName)sImplementationVisibility = JSC::ImplementationVisibility::%(visibility)s;" % data)
        lines.append("constinit const JSC::InlineAttribute s_%(codeName)sInlineAttribute = JSC::InlineAttribute::%(inlineAttribute)s;" % data)
        lines.append("constinit const int s_%(codeName)sLength = %(embeddedSourceLength)d;" % data)
        lines.append("static constinit const JSC::Intrinsic s_%(codeName)sIntrinsic = JSC::%(intrinsic)s;" % data)
        lines.append("constinit const char* const s_%(codeName)s =\n%(embeddedSource)s\n;" % data)
        return '\n'.join(lines)

    # Helper methods.

    @staticmethod
    def wrap_with_guard(guard, text):
        if not guard:
            return text
        return '\n'.join([
            '#if %s' % guard,
            text,
            '#endif // %s' % guard,
        ])

    @staticmethod
    def mangledNameForObject(object):
        if not isinstance(object, BuiltinObject):
            raise Exception("Invalid argument passed to mangledNameForObject()")

        def toCamel(match):
            str = match.group(0)
            return str[1].upper()
        return re.sub(r'\.[a-z]', toCamel, object.object_name, flags=re.IGNORECASE)


    @staticmethod
    def mangledNameForFunction(function):
        if not isinstance(function, BuiltinFunction):
            raise Exception("Invalid argument passed to mangledNameForFunction()")

        function_name = WK_ucfirst(function.function_name)

        def toCamel(match):
            str = match.group(0)
            return str[1].upper()
        function_name = re.sub(r'\.[a-z]', toCamel, function_name, flags=re.IGNORECASE)
        if function.is_constructor:
            function_name = function_name + "Constructor"

        object_name = BuiltinsGenerator.mangledNameForObject(function.object)
        return WK_lcfirst(object_name + function_name)
