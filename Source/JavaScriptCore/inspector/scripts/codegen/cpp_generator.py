#!/usr/bin/env python3
#
# Copyright (c) 2014-2018 Apple Inc. All rights reserved.
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

try:
    from .generator import ucfirst, Generator
    from .models import PrimitiveType, ObjectType, ArrayType, EnumType, AliasedType, Frameworks
except ImportError:
    from generator import ucfirst, Generator
    from models import PrimitiveType, ObjectType, ArrayType, EnumType, AliasedType, Frameworks

log = logging.getLogger('global')

_PRIMITIVE_TO_CPP_NAME_MAP = {
    'boolean': 'bool',
    'integer': 'int',
    'number': 'double',
    'string': 'String',
    'object': 'JSON::Object',
    'array': 'JSON::Array',
    'any': 'JSON::Value'
}

class CppGenerator(Generator):
    def __init__(self, *args, **kwargs):
        Generator.__init__(self, *args, **kwargs)

    def protocol_name(self):
        return self.model().framework.setting('cpp_protocol_group', '')

    def helpers_namespace(self):
        namespace = 'Helpers'
        if self.protocol_name() != 'Inspector':
            namespace = self.protocol_name() + namespace
        return namespace

    # Miscellaneous text manipulation routines.
    @staticmethod
    def cpp_getter_method_for_type(_type):
        if isinstance(_type, AliasedType):
            _type = _type.aliased_type
            # Fallthrough.

        if isinstance(_type, EnumType):
            _type = _type.primitive_type
            # Fallthrough.

        if isinstance(_type, ObjectType) or _type.qualified_name() in ['object']:
            return 'getObject'

        if isinstance(_type, ArrayType) or _type.qualified_name() in ['array']:
            return 'getArray'

        if isinstance(_type, PrimitiveType):
            if _type.raw_name() == 'number':
                return 'getDouble'
            if _type.raw_name() == 'any':
                return 'getValue'
            return 'get' + ucfirst(_type.raw_name())

        raise ValueError("unknown type")

    @staticmethod
    def cpp_setter_method_for_type(_type):
        if isinstance(_type, AliasedType):
            _type = _type.aliased_type
            # Fallthrough.

        if isinstance(_type, EnumType):
            _type = _type.primitive_type
            # Fallthrough.

        if isinstance(_type, ObjectType) or _type.qualified_name() in ['object']:
            return 'setObject'

        if isinstance(_type, ArrayType) or _type.qualified_name() in ['array']:
            return 'setArray'

        if isinstance(_type, PrimitiveType):
            if _type.raw_name() == 'number':
                return 'setDouble'
            if _type.raw_name() == 'any':
                return 'setValue'
            return 'set' + ucfirst(_type.raw_name())

        raise ValueError("unknown type")

    # Generate type representations for various situations.
    @staticmethod
    def cpp_protocol_type_for_type(_type):
        if isinstance(_type, AliasedType):
            _type = _type.aliased_type
            # Fallthrough.

        if isinstance(_type, ArrayType):
            if _type.raw_name() is None:
                return 'JSON::ArrayOf<%s>' % CppGenerator.cpp_protocol_type_for_type(_type.element_type)
            # Fallthrough.

        if isinstance(_type, (ObjectType, EnumType, ArrayType)):
            return 'Protocol::%s::%s' % (_type.type_domain().domain_name, _type.raw_name())

        if isinstance(_type, PrimitiveType):
            return CppGenerator.cpp_name_for_primitive_type(_type)

        raise ValueError("unknown type")

    @staticmethod
    def cpp_type_for_type_member_argument(_type, name):
        if isinstance(_type, AliasedType):
            _type = _type.aliased_type
            # Fallthrough.

        if isinstance(_type, (ArrayType, ObjectType)):
            return 'Ref<%s>&&' % CppGenerator.cpp_protocol_type_for_type(_type)

        if _type.qualified_name() == 'object':
            return 'Ref<JSON::Object>&&'

        if _type.qualified_name() == 'array':
            return 'Ref<JSON::Array>&&'

        if _type.qualified_name() == 'any':
            return 'Ref<JSON::Value>&&'

        if _type.qualified_name() == 'string':
            return 'const %s&' % CppGenerator.cpp_name_for_primitive_type(_type)

        if isinstance(_type, EnumType):
            if _type.is_anonymous:
                return ucfirst(name)
            return 'Protocol::%s::%s' % (_type.type_domain().domain_name, _type.raw_name())

        if isinstance(_type, PrimitiveType):
            return CppGenerator.cpp_name_for_primitive_type(_type)

        raise ValueError("unknown type")

    # In-parameters don't use builder types, because they could be passed
    # "open types" that are manually constructed out of InspectorObjects.
    @staticmethod
    def cpp_type_for_command_parameter(_type, is_optional):
        if isinstance(_type, AliasedType):
            _type = _type.aliased_type
            # Fallthrough.

        if isinstance(_type, EnumType) and _type.is_anonymous:
            _type = _type.primitive_type
            # Fallthrough.

        if isinstance(_type, ObjectType) or _type.qualified_name() == 'object':
            if is_optional:
                return 'RefPtr<JSON::Object>&&'
            return 'Ref<JSON::Object>&&'

        if isinstance(_type, ArrayType) or _type.qualified_name() == 'array':
            if is_optional:
                return 'RefPtr<JSON::Array>&&'
            return 'Ref<JSON::Array>&&'

        if _type.qualified_name() == 'any':
            if is_optional:
                return 'RefPtr<JSON::Value>&&'
            return 'Ref<JSON::Value>&&'

        if _type.qualified_name() == 'string':
            return 'const %s&' % CppGenerator.cpp_name_for_primitive_type(_type)

        if isinstance(_type, EnumType):
            cpp_name = 'Protocol::%s::%s' % (_type.type_domain().domain_name, _type.raw_name())
        elif isinstance(_type, PrimitiveType):
            cpp_name = CppGenerator.cpp_name_for_primitive_type(_type)
        else:
            raise ValueError("unknown type")

        if is_optional:
            cpp_name = 'std::optional<%s>&&' % cpp_name
        return cpp_name

    @staticmethod
    def cpp_type_for_command_return_declaration(_type, is_optional):
        if isinstance(_type, AliasedType):
            _type = _type.aliased_type
            # Fallthrough.

        if isinstance(_type, EnumType) and _type.is_anonymous:
            _type = _type.primitive_type
            # Fallthrough.

        if isinstance(_type, (ArrayType, ObjectType)):
            if is_optional:
                return 'RefPtr<%s>' % CppGenerator.cpp_protocol_type_for_type(_type)
            return 'Ref<%s>' % CppGenerator.cpp_protocol_type_for_type(_type)

        if _type.qualified_name() == 'object':
            if is_optional:
                return 'RefPtr<JSON::Object>'
            return 'Ref<JSON::Object>'

        if _type.qualified_name() == 'array':
            if is_optional:
                return 'RefPtr<JSON::Array>'
            return 'Ref<JSON::Array>'

        if _type.qualified_name() == 'any':
            if is_optional:
                return 'RefPtr<JSON::Value>'
            return 'Ref<JSON::Value>'

        if _type.qualified_name() == 'string':
            return CppGenerator.cpp_name_for_primitive_type(_type)

        if isinstance(_type, EnumType):
            cpp_name = 'Protocol::%s::%s' % (_type.type_domain().domain_name, _type.raw_name())
        elif isinstance(_type, PrimitiveType):
            cpp_name = CppGenerator.cpp_name_for_primitive_type(_type)
        else:
            raise ValueError("unknown type")

        if is_optional:
            cpp_name = 'std::optional<%s>' % cpp_name
        return cpp_name

    @staticmethod
    def cpp_type_for_command_return_argument(_type, is_optional):
        if isinstance(_type, AliasedType):
            _type = _type.aliased_type
            # Fallthrough.

        if isinstance(_type, EnumType) and _type.is_anonymous:
            _type = _type.primitive_type
            # Fallthrough.

        if isinstance(_type, (ArrayType, ObjectType)):
            if is_optional:
                return 'RefPtr<%s>&&' % CppGenerator.cpp_protocol_type_for_type(_type)
            return 'Ref<%s>&&' % CppGenerator.cpp_protocol_type_for_type(_type)

        if _type.qualified_name() == 'object':
            if is_optional:
                return 'RefPtr<JSON::Object>&&'
            return 'Ref<JSON::Object>&&'

        if _type.qualified_name() == 'array':
            if is_optional:
                return 'RefPtr<JSON::Array>&&'
            return 'Ref<JSON::Array>&&'

        if _type.qualified_name() == 'any':
            if is_optional:
                return 'RefPtr<JSON::Value>&&'
            return 'Ref<JSON::Value>&&'

        if _type.qualified_name() == 'string':
            return 'const %s&' % CppGenerator.cpp_name_for_primitive_type(_type)

        if isinstance(_type, EnumType):
            cpp_name = 'Protocol::%s::%s' % (_type.type_domain().domain_name, _type.raw_name())
        elif isinstance(_type, PrimitiveType):
            cpp_name = CppGenerator.cpp_name_for_primitive_type(_type)
        else:
            raise ValueError("unknown type")

        if is_optional:
            cpp_name = 'std::optional<%s>&&' % cpp_name
        return cpp_name

    @staticmethod
    def cpp_type_for_event_parameter(_type, is_optional, is_nullable=False, use_nullable_value=False):
        if isinstance(_type, AliasedType):
            _type = _type.aliased_type
            # Fallthrough.

        if isinstance(_type, EnumType) and _type.is_anonymous:
            _type = _type.primitive_type
            # Fallthrough.

        if isinstance(_type, (ArrayType, ObjectType)):
            if is_nullable or use_nullable_value:
                cpp_name = 'Ref<%s>' % CppGenerator.cpp_protocol_type_for_type(_type)
            elif is_optional:
                return 'RefPtr<%s>&&' % CppGenerator.cpp_protocol_type_for_type(_type)
            else:
                return 'Ref<%s>&&' % CppGenerator.cpp_protocol_type_for_type(_type)
        elif _type.qualified_name() == 'object':
            if is_nullable or use_nullable_value:
                cpp_name = 'Ref<JSON::Object>'
            elif is_optional:
                return 'RefPtr<JSON::Object>&&'
            else:
                return 'Ref<JSON::Object>&&'
        elif _type.qualified_name() == 'array':
            if is_nullable or use_nullable_value:
                cpp_name = 'Ref<JSON::Array>'
            elif is_optional:
                return 'RefPtr<JSON::Array>&&'
            else:
                return 'Ref<JSON::Array>&&'
        elif _type.qualified_name() == 'any':
            if is_nullable or use_nullable_value:
                cpp_name = 'Ref<JSON::Value>'
            elif is_optional:
                return 'RefPtr<JSON::Value>&&'
            else:
                return 'Ref<JSON::Value>&&'
        elif _type.qualified_name() == 'string':
            if is_nullable or use_nullable_value:
                cpp_name = CppGenerator.cpp_name_for_primitive_type(_type)
            else:
                return 'const %s&' % CppGenerator.cpp_name_for_primitive_type(_type)
        elif isinstance(_type, EnumType):
            cpp_name = 'Protocol::%s::%s' % (_type.type_domain().domain_name, _type.raw_name())
        elif isinstance(_type, PrimitiveType):
            cpp_name = CppGenerator.cpp_name_for_primitive_type(_type)
        else:
            raise ValueError("unknown type")

        if is_nullable:
            cpp_name = 'Nullable<%s>' % cpp_name
            if is_optional:
                return 'std::optional<%s>&&' % cpp_name
            return '%s&&' % cpp_name

        if is_optional and not use_nullable_value:
            cpp_name = 'std::optional<%s>&&' % cpp_name
        return cpp_name

    @staticmethod
    def cpp_resolved_event_parameter_type(_type):
        if isinstance(_type, AliasedType):
            _type = _type.aliased_type
            # Fallthrough.

        if isinstance(_type, EnumType) and _type.is_anonymous:
            _type = _type.primitive_type
            # Fallthrough.

        return _type

    @staticmethod
    def cpp_unwrapped_event_parameter_value(_type, parameter_name, is_optional):
        _type = CppGenerator.cpp_resolved_event_parameter_type(_type)
        parameter_value = parameter_name

        if _type.is_enum():
            if is_optional:
                return '*' + parameter_value
            return parameter_value
        if CppGenerator.should_release_argument(_type, is_optional):
            return parameter_value + '.releaseNonNull()'
        if CppGenerator.should_dereference_argument(_type, is_optional):
            return '*' + parameter_value
        if CppGenerator.should_move_argument(_type, is_optional):
            return 'WTF::move(%s)' % parameter_value
        return parameter_value

    def cpp_serialized_event_parameter_value(self, _type, parameter_name, is_optional):
        _type = CppGenerator.cpp_resolved_event_parameter_type(_type)
        parameter_value = CppGenerator.cpp_unwrapped_event_parameter_value(_type, parameter_name, is_optional)

        if _type.is_enum():
            return 'Protocol::%s::getEnumConstantValue(%s)' % (self.helpers_namespace(), parameter_value)
        return parameter_value

    @staticmethod
    def cpp_type_for_enum(enum_type, name):
        if enum_type.is_anonymous:
            return 'Protocol::%s::%s' % (enum_type.type_domain().domain_name, ucfirst(name))
        return 'Protocol::%s::%s' % (enum_type.type_domain().domain_name, enum_type.raw_name())

    @staticmethod
    def cpp_name_for_primitive_type(_type):
        return _PRIMITIVE_TO_CPP_NAME_MAP.get(_type.raw_name())

    @staticmethod
    def should_move_argument(_type, is_optional):
        if isinstance(_type, AliasedType):
            _type = _type.aliased_type
        if isinstance(_type, EnumType):
            _type = _type.primitive_type

        if _type.raw_name() == 'string':
            return False
        return is_optional == (_type.raw_name() in ['boolean', 'integer', 'number'])

    @staticmethod
    def should_release_argument(_type, is_optional):
        if isinstance(_type, AliasedType):
            _type = _type.aliased_type
        if isinstance(_type, EnumType):
            _type = _type.primitive_type

        if _type.raw_name() == 'string':
            return False
        if _type.raw_name() in ['boolean', 'integer', 'number']:
            return False
        return is_optional

    @staticmethod
    def should_dereference_argument(_type, is_optional):
        if isinstance(_type, AliasedType):
            _type = _type.aliased_type
        if isinstance(_type, EnumType):
            _type = _type.primitive_type

        if _type.raw_name() == 'string':
            return False
        if _type.raw_name() not in ['boolean', 'integer', 'number']:
            return False
        return is_optional
