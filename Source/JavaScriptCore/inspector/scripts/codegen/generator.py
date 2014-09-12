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
import os.path
import re
from string import Template

from generator_templates import GeneratorTemplates as Templates
from models import PrimitiveType, ObjectType, ArrayType, EnumType, AliasedType

log = logging.getLogger('global')


def ucfirst(str):
    return str[:1].upper() + str[1:]

_PRIMITIVE_TO_CPP_NAME_MAP = {
    'boolean': 'bool',
    'integer': 'int',
    'number': 'double',
    'string': 'String',
    'object': 'Inspector::InspectorObject',
    'any': 'Inspector::InspectorValue'
}

_ALWAYS_UPPERCASED_ENUM_VALUE_SUBSTRINGS = set(['API', 'CSS', 'DOM', 'HTML', 'XHR', 'XML'])

# These objects are built manually by creating and setting InspectorValues.
# Before sending these over the protocol, their shapes are checked against the specification.
# So, any types referenced by these types require debug-only assertions that check values.
# Calculating necessary assertions is annoying, and adds a lot of complexity to the generator.

# FIXME: This should be converted into a property in JSON.
_TYPES_NEEDING_RUNTIME_CASTS = set([
    "Runtime.RemoteObject",
    "Runtime.PropertyDescriptor",
    "Runtime.InternalPropertyDescriptor",
    "Debugger.FunctionDetails",
    "Debugger.CallFrame",
    "Canvas.TraceLog",
    "Canvas.ResourceInfo",
    "Canvas.ResourceState",
    # This should be a temporary hack. TimelineEvent should be created via generated C++ API.
    "Timeline.TimelineEvent",
    # For testing purposes only.
    "Test.TypeNeedingCast"
])

# FIXME: This should be converted into a property in JSON.
_TYPES_WITH_OPEN_FIELDS = set([
    "Timeline.TimelineEvent",
    # InspectorStyleSheet not only creates this property but wants to read it and modify it.
    "CSS.CSSProperty",
    # InspectorResourceAgent needs to update mime-type.
    "Network.Response",
    # For testing purposes only.
    "Test.OpenParameterBundle"
])


class Generator:
    def __init__(self, model, input_filepath):
        self._model = model
        self._input_filepath = input_filepath

    def model(self):
        return self._model

    def generate_license(self):
        return Template(Templates.CopyrightBlock).substitute(None, inputFilename=os.path.basename(self._input_filepath))

    # These methods are overridden by subclasses.
    def domains_to_generate(self):
        return filter(lambda domain: not domain.is_supplemental, self.model().domains)

    def generate_output(self):
        pass

    def output_filename(self):
        pass

    def encoding_for_enum_value(self, enum_value):
        if not hasattr(self, "_assigned_enum_values"):
            self._traverse_and_assign_enum_values()

        return self._enum_value_encodings[enum_value]

    def assigned_enum_values(self):
        if not hasattr(self, "_assigned_enum_values"):
            self._traverse_and_assign_enum_values()

        return self._assigned_enum_values[:]  # Slice.

    @staticmethod
    def type_needs_runtime_casts(_type):
        return _type.qualified_name() in _TYPES_NEEDING_RUNTIME_CASTS

    @staticmethod
    def type_has_open_fields(_type):
        return _type.qualified_name() in _TYPES_WITH_OPEN_FIELDS

    def type_needs_shape_assertions(self, _type):
        if not hasattr(self, "_types_needing_shape_assertions"):
            self.calculate_types_requiring_shape_assertions(self.model().domains)

        return _type in self._types_needing_shape_assertions

    # To restrict the domains over which we compute types needing assertions, call this method
    # before generating any output with the desired domains parameter. The computed
    # set of types will not be automatically regenerated on subsequent calls to
    # Generator.types_needing_shape_assertions().
    def calculate_types_requiring_shape_assertions(self, domains):
        domain_names = map(lambda domain: domain.domain_name, domains)
        log.debug("> Calculating types that need shape assertions (eligible domains: %s)" % ", ".join(domain_names))

        # Mutates the passed-in set; this simplifies checks to prevent infinite recursion.
        def gather_transitively_referenced_types(_type, gathered_types):
            if _type in gathered_types:
                return

            if isinstance(_type, ObjectType):
                log.debug("> Adding type %s to list of types needing shape assertions." % _type.qualified_name())
                gathered_types.add(_type)
                for type_member in _type.members:
                    gather_transitively_referenced_types(type_member.type, gathered_types)
            elif isinstance(_type, EnumType):
                log.debug("> Adding type %s to list of types needing shape assertions." % _type.qualified_name())
                gathered_types.add(_type)
            elif isinstance(_type, AliasedType):
                gather_transitively_referenced_types(_type.aliased_type, gathered_types)
            elif isinstance(_type, ArrayType):
                gather_transitively_referenced_types(_type.element_type, gathered_types)

        found_types = set()
        for domain in domains:
            for declaration in domain.type_declarations:
                if declaration.type.qualified_name() in _TYPES_NEEDING_RUNTIME_CASTS:
                    log.debug("> Gathering types referenced by %s to generate shape assertions." % declaration.type.qualified_name())
                    gather_transitively_referenced_types(declaration.type, found_types)

        self._types_needing_shape_assertions = found_types

    # Private helper instance methods.
    def _traverse_and_assign_enum_values(self):
        self._enum_value_encodings = {}
        self._assigned_enum_values = []
        all_types = []

        domains = self.domains_to_generate()

        for domain in domains:
            for type_declaration in domain.type_declarations:
                all_types.append(type_declaration.type)
                for type_member in type_declaration.type_members:
                    all_types.append(type_member.type)

        for domain in domains:
            for event in domain.events:
                all_types.extend([parameter.type for parameter in event.event_parameters])

        for domain in domains:
            for command in domain.commands:
                all_types.extend([parameter.type for parameter in command.call_parameters])
                all_types.extend([parameter.type for parameter in command.return_parameters])

        for _type in all_types:
            if not isinstance(_type, EnumType):
                continue
            map(self._assign_encoding_for_enum_value, _type.enum_values())

    def _assign_encoding_for_enum_value(self, enum_value):
        if enum_value in self._enum_value_encodings:
            return

        self._enum_value_encodings[enum_value] = len(self._assigned_enum_values)
        self._assigned_enum_values.append(enum_value)

    # Miscellaneous text manipulation routines.
    @staticmethod
    def wrap_with_guard_for_domain(domain, text):
        guard = domain.feature_guard
        if guard is not None:
            lines = [
                '#if %s' % guard,
                text,
                '#endif // %s' % guard
                ]
            return '\n'.join(lines)
        else:
            return text

    @staticmethod
    def stylized_name_for_enum_value(enum_value):
        regex = '(%s)' % "|".join(_ALWAYS_UPPERCASED_ENUM_VALUE_SUBSTRINGS)

        def replaceCallback(match):
            return match.group(1).upper()

        # Split on hyphen, introduce camelcase, and force uppercasing of acronyms.
        subwords = map(ucfirst, enum_value.split('-'))
        return re.sub(re.compile(regex, re.IGNORECASE), replaceCallback, "".join(subwords))

    @staticmethod
    def keyed_get_method_for_type(_type):
        if isinstance(_type, ObjectType):
            return 'getObject'
        if isinstance(_type, ArrayType):
            return 'getArray'
        if isinstance(_type, PrimitiveType):
            if _type.raw_name() is 'integer':
                return 'getInteger'
            elif _type.raw_name() is 'number':
                return 'getDouble'
            else:
                return 'get' + ucfirst(_type.raw_name())
        if isinstance(_type, AliasedType):
            return Generator.keyed_get_method_for_type(_type.aliased_type)
        if isinstance(_type, EnumType):
            return Generator.keyed_get_method_for_type(_type.primitive_type)

    @staticmethod
    def keyed_set_method_for_type(_type):
        if isinstance(_type, ObjectType):
            return 'setObject'
        if isinstance(_type, ArrayType):
            return 'setArray'
        if isinstance(_type, PrimitiveType):
            if _type.raw_name() is 'integer':
                return 'setInteger'
            elif _type.raw_name() is 'number':
                return 'setDouble'
            elif _type.raw_name() is 'any':
                return 'setValue'
            else:
                return 'set' + ucfirst(_type.raw_name())
        if isinstance(_type, AliasedType):
            return Generator.keyed_set_method_for_type(_type.aliased_type)
        if isinstance(_type, EnumType):
            return Generator.keyed_set_method_for_type(_type.primitive_type)

    # Generate type representations for various situations.
    @staticmethod
    def protocol_type_string_for_type(_type):
        if isinstance(_type, ObjectType) and len(_type.members) == 0:
            return 'Inspector::InspectorObject'
        if isinstance(_type, (ObjectType, AliasedType, EnumType)):
            return 'Inspector::Protocol::%s::%s' % (_type.type_domain().domain_name, _type.raw_name())
        if isinstance(_type, ArrayType):
            return 'Inspector::Protocol::Array<%s>' % Generator.protocol_type_string_for_type(_type.element_type)
        if isinstance(_type, PrimitiveType):
            return Generator.cpp_name_for_primitive_type(_type)

    @staticmethod
    def protocol_type_string_for_type_member(type_member, object_declaration):
        if isinstance(type_member.type, EnumType) and type_member.type.is_anonymous:
            return '::'.join([Generator.protocol_type_string_for_type(object_declaration.type), ucfirst(type_member.member_name)])
        else:
            return Generator.protocol_type_string_for_type(type_member.type)

    @staticmethod
    def type_string_for_unchecked_formal_in_parameter(parameter):
        _type = parameter.type
        if isinstance(_type, AliasedType):
            _type = _type.aliased_type  # Fall through to enum or primitive.

        if isinstance(_type, EnumType):
            _type = _type.primitive_type  # Fall through to primitive.

        sigil = '*' if parameter.is_optional else '&'
        # This handles the 'any' type and objects with defined properties.
        if isinstance(_type, ObjectType) or _type.qualified_name() is 'object':
            return 'const RefPtr<Inspector::InspectorObject>' + sigil
        if isinstance(_type, ArrayType):
            return 'const RefPtr<Inspector::InspectorArray>' + sigil
        if isinstance(_type, PrimitiveType):
            cpp_name = Generator.cpp_name_for_primitive_type(_type)
            if parameter.is_optional:
                return 'const %s*' % cpp_name
            elif _type.raw_name() in ['string']:
                return 'const %s&' % cpp_name
            else:
                return cpp_name

        return "unknown_unchecked_formal_in_parameter_type"

    @staticmethod
    def type_string_for_checked_formal_event_parameter(parameter):
        return Generator.type_string_for_type_with_name(parameter.type, parameter.parameter_name, parameter.is_optional)

    @staticmethod
    def type_string_for_type_member(member):
        return Generator.type_string_for_type_with_name(member.type, member.member_name, False)

    @staticmethod
    def type_string_for_type_with_name(_type, type_name, is_optional):
        if isinstance(_type, (ArrayType, ObjectType)):
            return 'PassRefPtr<%s>' % Generator.protocol_type_string_for_type(_type)
        if isinstance(_type, AliasedType):
            builder_type = Generator.protocol_type_string_for_type(_type)
            if is_optional:
                return 'const %s* const' % builder_type
            elif _type.aliased_type.qualified_name() in ['integer', 'number']:
                return Generator.cpp_name_for_primitive_type(_type.aliased_type)
            elif _type.aliased_type.qualified_name() in ['string']:
                return 'const %s&' % builder_type
            else:
                return builder_type
        if isinstance(_type, PrimitiveType):
            cpp_name = Generator.cpp_name_for_primitive_type(_type)
            if _type.qualified_name() in ['object']:
                return 'PassRefPtr<Inspector::InspectorObject>'
            elif _type.qualified_name() in ['any']:
                return 'PassRefPtr<Inspector::InspectorValue>'
            elif is_optional:
                return 'const %s* const' % cpp_name
            elif _type.qualified_name() in ['string']:
                return 'const %s&' % cpp_name
            else:
                return cpp_name
        if isinstance(_type, EnumType):
            if _type.is_anonymous:
                enum_type_name = ucfirst(type_name)
            else:
                enum_type_name = 'Inspector::Protocol::%s::%s' % (_type.type_domain().domain_name, _type.raw_name())

            if is_optional:
                return '%s*' % enum_type_name
            else:
                return '%s' % enum_type_name

    @staticmethod
    def type_string_for_formal_out_parameter(parameter):
        _type = parameter.type

        if isinstance(_type, AliasedType):
            _type = _type.aliased_type  # Fall through.

        if isinstance(_type, (ObjectType, ArrayType)):
            return 'RefPtr<%s>&' % Generator.protocol_type_string_for_type(_type)
        if isinstance(_type, PrimitiveType):
            cpp_name = Generator.cpp_name_for_primitive_type(_type)
            if parameter.is_optional:
                return "Inspector::Protocol::OptOutput<%s>*" % cpp_name
            else:
                return '%s*' % cpp_name
        if isinstance(_type, EnumType):
            if _type.is_anonymous:
                return 'Inspector%sBackendDispatcherHandler::%s*' % (_type.type_domain().domain_name, ucfirst(parameter.parameter_name))
            else:
                return 'Inspector::Protocol::%s::%s*' % (_type.type_domain().domain_name, _type.raw_name())

        raise ValueError("unknown formal out parameter type.")

    # FIXME: this is only slightly different from out parameters; they could be unified.
    @staticmethod
    def type_string_for_formal_async_parameter(parameter):
        _type = parameter.type
        if isinstance(_type, AliasedType):
            _type = _type.aliased_type  # Fall through.

        if isinstance(_type, EnumType):
            _type = _type.primitive_type  # Fall through.

        if isinstance(_type, (ObjectType, ArrayType)):
            return 'PassRefPtr<%s>' % Generator.protocol_type_string_for_type(_type)
        if isinstance(_type, PrimitiveType):
            cpp_name = Generator.cpp_name_for_primitive_type(_type)
            if parameter.is_optional:
                return "Inspector::Protocol::OptOutput<%s>*" % cpp_name
            elif _type.qualified_name() in ['integer', 'number']:
                return Generator.cpp_name_for_primitive_type(_type)
            elif _type.qualified_name() in ['string']:
                return 'const %s&' % cpp_name
            else:
                return cpp_name

        raise ValueError("Unknown formal async parameter type.")

    # In-parameters don't use builder types, because they could be passed
    # "open types" that are manually constructed out of InspectorObjects.

    # FIXME: Only parameters that are actually open types should need non-builder parameter types.
    @staticmethod
    def type_string_for_stack_in_parameter(parameter):
        _type = parameter.type
        if isinstance(_type, AliasedType):
            _type = _type.aliased_type  # Fall through.

        if isinstance(_type, EnumType):
            _type = _type.primitive_type  # Fall through.

        if isinstance(_type, ObjectType):
            return "RefPtr<Inspector::InspectorObject>"
        if isinstance(_type, ArrayType):
            return "RefPtr<Inspector::InspectorArray>"
        if isinstance(_type, PrimitiveType):
            cpp_name = Generator.cpp_name_for_primitive_type(_type)
            if _type.qualified_name() in ['any', 'object']:
                return "RefPtr<%s>" % Generator.cpp_name_for_primitive_type(_type)
            elif parameter.is_optional and _type.qualified_name() not in ['boolean', 'string', 'integer']:
                return "Inspector::Protocol::OptOutput<%s>" % cpp_name
            else:
                return cpp_name

    @staticmethod
    def type_string_for_stack_out_parameter(parameter):
        _type = parameter.type
        if isinstance(_type, (ArrayType, ObjectType)):
            return 'RefPtr<%s>' % Generator.protocol_type_string_for_type(_type)
        if isinstance(_type, AliasedType):
            builder_type = Generator.protocol_type_string_for_type(_type)
            if parameter.is_optional:
                return "Inspector::Protocol::OptOutput<%s>" % builder_type
            return '%s' % builder_type
        if isinstance(_type, PrimitiveType):
            cpp_name = Generator.cpp_name_for_primitive_type(_type)
            if parameter.is_optional:
                return "Inspector::Protocol::OptOutput<%s>" % cpp_name
            else:
                return cpp_name
        if isinstance(_type, EnumType):
            if _type.is_anonymous:
                return 'Inspector%sBackendDispatcherHandler::%s' % (_type.type_domain().domain_name, ucfirst(parameter.parameter_name))
            else:
                return 'Inspector::Protocol::%s::%s' % (_type.type_domain().domain_name, _type.raw_name())

    @staticmethod
    def assertion_method_for_type_member(type_member, object_declaration):

        def assertion_method_for_type(_type):
            return 'BindingTraits<%s>::assertValueHasExpectedType' % Generator.protocol_type_string_for_type(_type)

        if isinstance(type_member.type, AliasedType):
            return assertion_method_for_type(type_member.type.aliased_type)
        if isinstance(type_member.type, EnumType) and type_member.type.is_anonymous:
            return 'BindingTraits<%s>::assertValueHasExpectedType' % Generator.protocol_type_string_for_type_member(type_member, object_declaration)

        return assertion_method_for_type(type_member.type)

    @staticmethod
    def cpp_name_for_primitive_type(_type):
        return _PRIMITIVE_TO_CPP_NAME_MAP.get(_type.raw_name())

    @staticmethod
    def js_name_for_parameter_type(_type):
        _type = _type
        if isinstance(_type, AliasedType):
            _type = _type.aliased_type  # Fall through.
        if isinstance(_type, EnumType):
            _type = _type.primitive_type  # Fall through.

        if isinstance(_type, (ArrayType, ObjectType)):
            return 'object'
        if isinstance(_type, PrimitiveType):
            if _type.qualified_name() in ['object', 'any']:
                return 'object'
            elif _type.qualified_name() in ['integer', 'number']:
                return 'number'
            else:
                return _type.qualified_name()

    # Decide whether certain helpers are necessary in a situation.
    @staticmethod
    def should_use_wrapper_for_return_type(_type):
        return not isinstance(_type, (ArrayType, ObjectType))

    @staticmethod
    def should_pass_by_copy_for_return_type(_type):
        return isinstance(_type, (ArrayType, ObjectType)) or (isinstance(_type, (PrimitiveType)) and _type.qualified_name() == "object")
