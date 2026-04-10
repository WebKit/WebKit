#
# Copyright (C) 2022-2024 Apple Inc. All rights reserved.
# Copyright (C) 2024 Sony Interactive Entertainment Inc.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import copy
import re


class Template(object):
    def __init__(self, template_type, namespace, name, enum_storage=None):
        self.type = template_type
        self.namespace = namespace
        self.name = name
        self.enum_storage = enum_storage

    def forward_declaration(self):
        if self.enum_storage:
            return f'{self.type} {self.name} : {self.enum_storage};'
        return f'{self.type} {self.name};'

    def specialization(self):
        return f'{self.namespace}::{self.name}'


class SerializedType(object):
    def __init__(self, struct_or_class, cf_type, namespace, name, parent_class_name, members, dictionary_members, condition, attributes, templates, other_metadata=None):
        self.struct_or_class = struct_or_class
        self.cf_type = cf_type
        self.to_cf_method = None
        self.from_cf_method = None
        self.forward_declaration = None
        self.custom_secure_coding_class = None
        self.namespace = namespace
        self.name = name
        self.parent_class_name = parent_class_name
        self.parent_class = None
        self.members = members
        self.dictionary_members = dictionary_members
        self.alias = None
        self.condition = condition
        self.encoders = ['Encoder']
        self.return_ref = False
        self.construct_subclass = None
        self.create_using = False
        self.populate_from_empty_constructor = False
        self.nested = False
        self.rvalue = False
        self.webkit_platform = False
        self.members_are_subclasses = False
        self.custom_encoder = False
        self.support_wkkeyedcoder = False
        self.disableMissingMemberCheck = False
        self.debug_decoding_failure = False
        self.generic_wrapper = None
        if attributes is not None:
            for attribute in attributes.split(', '):
                if '=' in attribute:
                    key, value = attribute.split('=')
                    if key == 'AdditionalEncoder':
                        self.encoders.append(value)
                    elif key == 'ConstructSubclass':
                        self.construct_subclass = value
                    elif key == 'CreateUsing':
                        self.create_using = value
                    elif key == 'Alias':
                        self.alias = value
                    elif key == 'ToCFMethod':
                        self.to_cf_method = value
                    elif key == 'FromCFMethod':
                        self.from_cf_method = value
                    elif key == 'ForwardDeclaration':
                        self.forward_declaration = value
                    elif key == 'WebKitSecureCodingClass':
                        self.custom_secure_coding_class = value
                    elif key == 'Wrapper':
                        self.generic_wrapper = value
                    else:
                        raise Exception(f'Invalid attribute ({key}={value}) found on struct: {self.namespace}::{self.name}')
                else:
                    if attribute == 'Nested':
                        self.nested = True
                    elif attribute == 'RefCounted':
                        self.return_ref = True
                    elif attribute == 'DisableMissingMemberCheck':
                        self.disableMissingMemberCheck = True
                    elif attribute == 'RValue':
                        self.rvalue = True
                    elif attribute == 'WebKitPlatform':
                        self.webkit_platform = True
                    elif attribute == 'LegacyPopulateFromEmptyConstructor':
                        self.populate_from_empty_constructor = True
                    elif attribute == 'CustomEncoder':
                        self.custom_encoder = True
                    elif attribute == 'SupportWKKeyedCoder':
                        self.support_wkkeyedcoder = True
                    elif attribute == 'DebugDecodingFailure':
                        self.debug_decoding_failure = True
                    elif attribute in ['CustomHeader']:
                        pass
                    else:
                        raise Exception(f'Invalid attribute ({attribute}) found on struct: {self.namespace}::{self.name}')
        self.templates = templates
        if other_metadata:
            if other_metadata == 'subclasses':
                self.members_are_subclasses = True
        if self.is_webkit_secure_coding_type():
            self.namespace = 'WebKit'
            self.webkit_platform = True

    def name_as_identifier(self):
        return re.sub(r'\W+', '_', self.name)

    def namespace_and_name(self):
        if self.cf_type is not None:
            return f'{self.cf_type}Ref'
        if self.namespace is None:
            return self.name
        return f'{self.namespace}::{self.cpp_struct_or_class_name()}'

    def namespace_if_not_wtf_and_name(self):
        if self.namespace == 'WTF':
            return self.name
        if self.namespace is None:
            return self.name
        return f'{self.namespace}::{self.cpp_struct_or_class_name()}'

    def namespace_and_name_for_construction(self, specialization):
        fulltype = None
        if self.construct_subclass:
            fulltype = f'{self.namespace}::{self.construct_subclass}'
        elif self.generic_wrapper is not None:
            fulltype = self.generic_wrapper
        else:
            fulltype = self.namespace_and_name()
        if specialization:
            fulltype = f'{fulltype}<{specialization}>'
        return fulltype

    def cf_wrapper_type(self):
        return f'{self.namespace}::{self.name}'

    def name_declaration_for_serialized_type_info(self):
        if self.cf_type is not None:
            return f'{self.cf_type}Ref'
        if self.namespace == 'WTF':
            if self.name != "UUID":
                return self.name
        return self.namespace_and_name()

    def subclass_enum_name(self):
        result = ""
        if self.namespace:
            result += self.namespace + "_"
        return f'{result}{self.name}_Subclass'

    def function_name_for_enum(self):
        return 'isValidEnum'

    def can_assert_member_order_is_correct(self):
        if self.disableMissingMemberCheck:
            return False
        for member in self.members:
            if '()' in member.name:
                return False
            if '.' in member.name:
                return False
        return True

    def members_for_serialized_type_info(self):
        return self.serialized_members()

    def serialized_members(self):
        return list(filter(lambda member: 'NotSerialized' not in member.attributes, self.members))

    def has_optional_tuple_bits(self):
        if len(self.members) == 0:
            return False
        for member in self.members:
            if 'OptionalTupleBits' in member.attributes:
                return True
        return False

    def should_skip_forward_declare(self):
        return self.nested or self.templates

    def cpp_type_from_struct_or_class(self):
        if self.is_webkit_secure_coding_type():
            return 'class'
        return self.struct_or_class

    def cpp_struct_or_class_name(self):
        if self.is_webkit_secure_coding_type():
            return f'CoreIPC{self.name}'
        return self.name

    def is_webkit_secure_coding_type(self):
        return self.struct_or_class == 'webkit_secure_coding'

    def wrapper_for_webkit_secure_coding_type(self):
        copied_type = copy.copy(self)
        copied_type.struct_or_class = 'class'
        copied_type.name = self.cpp_struct_or_class_name()
        copied_type.dictionary_members = None
        return copied_type


class SerializedEnum(object):
    def __init__(self, namespace, name, underlying_type, valid_values, condition, attributes):
        self.namespace = namespace
        self.name = name
        self.underlying_type = underlying_type
        self.valid_values = valid_values
        self.condition = condition
        self.attributes = attributes

    def namespace_and_name(self):
        if self.namespace is None:
            return self.name
        return f'{self.namespace}::{self.name}'

    def function_name(self):
        if self.is_option_set():
            return 'isValidOptionSet'
        return 'isValidEnum'

    def parameter(self):
        if self.is_option_set():
            return f'OptionSet<{self.namespace_and_name()}>'
        return self.underlying_type

    def is_option_set(self):
        return 'OptionSet' in self.attributes

    def is_nested(self):
        return 'Nested' in self.attributes

    def is_webkit_platform(self):
        return 'WebKitPlatform' in self.attributes


class MemberVariable(object):
    def __init__(self, type, name, condition, attributes, namespace=None, is_subclass=False):
        assert type == type.strip(), f"MemberVariable({type} {name}) has invalid type '{type}'"
        assert name == name.strip(), f"MemberVariable({type} {name}) has invalid name '{name}'"
        self.type = type
        self.name = name
        self.condition = condition
        self.attributes = attributes
        self.namespace = namespace
        self.is_subclass = is_subclass

    def optional_tuple_bit(self):
        for attribute in self.attributes:
            match = re.search(r'OptionalTupleBit=(.*)', attribute)
            if match:
                bit, = match.groups()
                return bit
        return None

    def optional_tuple_bits(self):
        for attribute in self.attributes:
            if attribute == 'OptionalTupleBits':
                return True
        return False

    def value_without_question_mark(self):
        value = self.name
        if self.value_is_optional():
            value = value[:-1]
        return value

    def ns_type_enum_value(self):
        value = self.value_without_question_mark()
        if value.startswith('Dictionary'):
            return 'Dictionary'
        if value.startswith('Array'):
            return 'Array'
        return value

    def array_contents(self):
        value = self.value_without_question_mark()
        if not value.startswith('Array'):
            return None
        match = re.search(r'(.*)<(.*)>', value)
        if match:
            array, contents = match.groups()
            if contents == 'String':
                return 'NSString'
            if contents == 'Data':
                return 'NSData'
            return contents
        return None

    def dictionary_contents(self):
        value = self.value_without_question_mark()
        if not value.startswith('Dictionary'):
            return None
        match = re.search(r'(.*)<(.*)>', value)
        if match:
            dictionary, contents = match.groups()
            match = re.search(r'(.*), (.*)', contents)
            if match:
                keys, values = match.groups()
                assert keys == 'String'
                return values
        return None

    def has_container_contents(self):
        return self.dictionary_contents() is not None or self.array_contents() is not None

    def ns_type(self):
        value = self.value_without_question_mark()
        if value == 'String':
            return 'NSString'
        if value.startswith('Dictionary'):
            return 'NSDictionary'
        if value == 'Data':
            return 'NSData'
        if value == 'Date':
            return 'NSDate'
        if value == 'Number':
            return 'NSNumber'
        if value == 'URL':
            return 'NSURL'
        if value == 'PersonNameComponents':
            return 'NSPersonNameComponents'
        if value.startswith('Array'):
            return 'NSArray'
        if value == 'Set':
            return 'NSSet'
        return value

    def ns_type_pointer(self):
        value = self.ns_type()
        if value == 'SecTrustRef':
            return value
        return f'{value} *'

    def type_check(self):
        value = self.ns_type()
        if value == 'SecTrustRef':
            return f'(m_{self.type} && CFGetTypeID((CFTypeRef)m_{self.type}.get()) == SecTrustGetTypeID())'
        return f'[m_{self.type} isKindOfClass:IPC::getClass<{value}>()]'

    def id_cast(self):
        value = self.ns_type()
        if value == 'SecTrustRef':
            return '(id)'
        return ''

    def dictionary_type(self):
        prefix = 'std::optional<' if self.value_is_optional() else ''
        suffix = '>' if self.value_is_optional() else ''
        if self.array_contents() is not None:
            return f'{prefix}Vector<RetainPtr<{self.array_contents()}>>{suffix}'
        if self.dictionary_contents() is not None:
            return f'{prefix}Vector<std::pair<String, RetainPtr<{self.dictionary_contents()}>>>{suffix}'
        return f'RetainPtr<{self.ns_type()}>'

    def value_is_optional(self):
        return self.name.endswith('?')


class EnumMember(object):
    def __init__(self, name, condition):
        self.name = name
        self.condition = condition


class ConditionalForwardDeclaration(object):
    def __init__(self, declaration, condition):
        self.declaration = declaration
        self.condition = condition

    def __lt__(self, other):
        if self.declaration != other.declaration:
            return self.declaration < other.declaration

        def condition_str(condition):
            return "" if condition is None else condition

        return condition_str(self.condition) < condition_str(other.condition)

    def __eq__(self, other):
        return other and self.declaration == other.declaration and self.condition == other.condition

    def __hash__(self):
        return hash((self.declaration, self.condition))


class ConditionalHeader(object):
    def __init__(self, header, condition, webkit_platform=False, secure_coding=False):
        self.header = header
        self.condition = condition
        self.webkit_platform = webkit_platform
        self.secure_coding = secure_coding

    def __lt__(self, other):
        if self.header != other.header:
            return self.header < other.header

        def condition_str(condition):
            return "" if condition is None else condition

        return condition_str(self.condition) < condition_str(other.condition)

    def __eq__(self, other):
        return other and self.header == other.header and self.condition == other.condition and self.webkit_platform == other.webkit_platform

    def __hash__(self):
        return hash((self.header, self.condition))


class UsingStatement(object):
    def __init__(self, name, alias_lines, condition):
        self.name = name
        self.alias_lines = alias_lines
        self.condition = condition


class ObjCWrappedType(object):
    def __init__(self, ns_type, wrapper, condition):
        self.ns_type = ns_type
        self.wrapper = wrapper
        self.condition = condition


class ConditionStackEntry(object):
    def __init__(self, expression):
        self._base_expression = expression
        self.should_negate = False

    @property
    def expression(self):
        return self._base_expression if not self.should_negate else f'!({self._base_expression})'


def generate_condition_expression(condition_stack):
    if not condition_stack:
        return None

    full_condition_expression = condition_stack[0].expression
    if len(condition_stack) == 1:
        return full_condition_expression

    for condition in condition_stack[1:]:
        condition_expression = condition.expression
        full_condition_expression = f'({full_condition_expression}) && ({condition_expression})'

    return full_condition_expression


def parse_serialized_types(file):
    serialized_types = []
    serialized_enums = []
    using_statements = []
    objc_wrapped_types = []
    additional_forward_declarations = []
    headers = []

    attributes = None
    namespace = None
    name = None
    members = []
    dictionary_members = []
    type_condition = None
    member_condition = None
    type_condition_stack = []
    member_condition_stack = []
    struct_or_class = None
    cf_type = None
    underlying_type = None
    parent_class_name = None
    metadata = None
    templates = []

    file_lines = []
    for line in file:
        file_lines.append(line.strip())

    for line_number in range(len(file_lines)):
        line = file_lines[line_number]
        if line.startswith('#'):
            if line == '#else':
                if name is None:
                    if type_condition_stack:
                        type_condition_stack[-1].should_negate = True
                else:
                    if member_condition_stack:
                        member_condition_stack[-1].should_negate = True
            elif line.startswith('#if '):
                condition_expression = line[4:]
                if name is None:
                    type_condition_stack.append(ConditionStackEntry(expression=condition_expression))
                else:
                    member_condition_stack.append(ConditionStackEntry(expression=condition_expression))
            elif line.startswith('#endif'):
                if name is None:
                    if type_condition_stack:
                        type_condition_stack.pop()
                else:
                    if member_condition_stack:
                        member_condition_stack.pop()
            type_condition = generate_condition_expression(type_condition_stack)
            member_condition = generate_condition_expression(member_condition_stack)
            continue
        if line.startswith('}'):
            if underlying_type is not None:
                serialized_enums.append(SerializedEnum(namespace, name, underlying_type, members, type_condition, attributes))
            else:
                type = SerializedType(struct_or_class, cf_type, namespace, name, parent_class_name, members, dictionary_members, type_condition, attributes, templates, metadata)
                serialized_types.append(type)
                if namespace is not None and (attributes is None or ('CustomHeader' not in attributes and 'Nested' not in attributes)):
                    if namespace == 'WebKit' or namespace == 'WebKit::WebPushD':
                        headers.append(ConditionalHeader(f'"{name}.h"', type_condition))
                    elif namespace == 'WTF':
                        headers.append(ConditionalHeader(f'<wtf/{name}.h>', type_condition))
                    elif namespace == 'WebKit::WebGPU':
                        headers.append(ConditionalHeader(f'"WebGPU{name}.h"', type_condition))
                    else:
                        headers.append(ConditionalHeader(f'<{namespace}/{name}.h>', type_condition))
            attributes = None
            namespace = None
            name = None
            members = []
            dictionary_members = []
            member_condition = None
            struct_or_class = None
            cf_type = None
            underlying_type = None
            parent_class_name = None
            metadata = None
            templates = []
            continue

        match = re.search(r'^headers?: (.*)', line)
        if match:
            for header in match.group(1).split():
                headers.append(ConditionalHeader(header, type_condition))
            continue
        match = re.search(r'^template: (enum class) (.*)::(.*) : (.*)', line)
        if match:
            template_type, template_namespace, template_name, enum_storage = match.groups()
            templates.append(Template(template_type, template_namespace, template_name, enum_storage))
            continue
        match = re.search(r'^template: (struct|class|enum class) (.*)::(.*)', line)
        if match:
            template_type, template_namespace, template_name = match.groups()
            templates.append(Template(template_type, template_namespace, template_name, None))
            continue
        match = re.search(r'webkit_platform_headers?: (.*)', line)
        if match:
            for header in match.group(1).split():
                headers.append(ConditionalHeader(header, type_condition, True))
            continue
        match = re.search(r'secure_coding_headers?: (.*)', line)
        if match:
            for header in match.group(1).split():
                headers.append(ConditionalHeader(header, type_condition, False, True))
            continue

        match = re.search(r'(.*)enum class (.*)::(.*) : (.*) {', line)
        if match:
            attributes, namespace, name, underlying_type = match.groups()
            assert underlying_type != 'bool'
            continue
        match = re.search(r'(.*)enum class (.*)::(.*) : bool', line)
        if match:
            serialized_enums.append(SerializedEnum(match.groups()[1], match.groups()[2], 'bool', [], type_condition, match.groups()[0]))
            continue
        match = re.search(r'(.*)enum class (.*) : (.*) {', line)
        if match:
            attributes, name, underlying_type = match.groups()
            assert underlying_type != 'bool'
            continue

        match = re.search(r'\[(.*)\] (struct|class|alias) (.*)::(.*) : (.*) {', line)
        if match:
            attributes, struct_or_class, namespace, name, parent_class_name = match.groups()
            continue
        match = re.search(r'(struct|class|alias) (.*)::(.*) : (.*) {', line)
        if match:
            struct_or_class, namespace, name, parent_class_name = match.groups()
            continue
        match = re.search(r'\[(.*)\] (struct|class|alias) (.*)::((?:.*)<(?:.*)>) {', line)
        if match:
            attributes, struct_or_class, namespace, name = match.groups()
            continue
        match = re.search(r'\[(.*)\] (struct|class|alias) (.*)::([^\s]*) {', line)
        if match:
            attributes, struct_or_class, namespace, name = match.groups()
            continue
        match = re.search(r'\[(.*)\] (struct|class) (.*)::(.*)\s+(.*) {', line)
        if match:
            attributes, struct_or_class, namespace, name, metadata = match.groups()
            continue
        match = re.search(r'(struct|class|alias) (.*)::(.*) {', line)
        if match:
            struct_or_class, namespace, name = match.groups()
            continue
        match = re.search(r'\[(.*)\] (struct|class|alias|webkit_secure_coding) (.*) {', line)
        if match:
            attributes, struct_or_class, name = match.groups()
            continue
        match = re.search(r'(struct|class|alias|webkit_secure_coding) (.*) {', line)
        if match:
            struct_or_class, name = match.groups()
            continue
        match = re.search(r'\[(.*)\] (.*)Ref wrapped by (.*)::(.*) {', line)
        if match:
            attributes, cf_type, namespace, name = match.groups()
            continue
        match = re.search(r'(.*)Ref wrapped by (.*)::(.*) {', line)
        if match:
            cf_type, namespace, name = match.groups()
            continue
        match = re.search(r'(.*) wrapped by (.*)', line)
        if match:
            objc_wrapped_type, objc_wrapper = match.groups()
            objc_wrapped_types.append(ObjCWrappedType(objc_wrapped_type, objc_wrapper, type_condition))
            continue
        match = re.search(r'additional_forward_declaration: (.*)', line)
        if match:
            declaration = match.groups()[0]
            additional_forward_declarations.append(ConditionalForwardDeclaration(declaration, type_condition))
            continue
        match = re.search(r'using (.*) = Variant<$', line)
        if match:
            line_number = line_number + 1
            alias_lines = ['Variant<']
            while not file_lines[line_number].startswith('>'):
                alias_lines.append('    ' + file_lines[line_number])
                line_number = line_number + 1
            alias_lines.append('>')
            using_statements.append(UsingStatement(match.groups()[0], alias_lines, type_condition))
            continue
        match = re.search(r'using (.*) = ([^;]*)', line)
        if match:
            using_statements.append(UsingStatement(match.groups()[0], [match.groups()[1]], type_condition))
            continue
        if underlying_type is not None:
            members.append(EnumMember(line.strip(' ,'), member_condition))
            continue
        elif metadata == 'subclasses':
            match = re.search(r'(.*)::(.*)', line.strip(' ,'))
            if match:
                subclass_namespace, subclass_name = match.groups()
                subclass_member = MemberVariable("subclass", subclass_name, member_condition, [], namespace=subclass_namespace, is_subclass=True)
                members.append(subclass_member)
            continue

        if struct_or_class == 'webkit_secure_coding':
            match = re.search(r'\[(.*)\] (.*): ([^;]*)', line)
        else:
            match = re.search(r'\[(.*)\] (.*) ([^;]*)', line)
        if match:
            member_attributes_s, member_type, member_name = match.groups()
            member_attributes = []
            match = re.search(r"((, |^)+(Validator='.*?'))(, |$)?", member_attributes_s)
            if match:
                complete, _, validator, _ = match.groups()
                member_attributes.append(validator)
                member_attributes_s = member_attributes_s.replace(complete, "")
            match = re.search(r"((, |^)+(SecureCodingAllowed=\[.*?\]))(, |$)?", member_attributes_s)
            if match:
                complete, _, allow_list, _ = match.groups()
                member_attributes.append(allow_list)
                member_attributes_s = member_attributes_s.replace(complete, "")
            member_attributes += [member_attribute.strip() for member_attribute in member_attributes_s.split(",")]
            if struct_or_class == 'webkit_secure_coding':
                dictionary_members.append(MemberVariable(member_type, member_name, member_condition, member_attributes))
            else:
                members.append(MemberVariable(member_type, member_name, member_condition, member_attributes))
        else:
            if struct_or_class == 'webkit_secure_coding':
                match = re.search(r'(.*): ([^;]*)', line)
            else:
                match = re.search(r'(.*) ([^;]*)', line)
            if match:
                member_type, member_name = match.groups()
                if struct_or_class == 'webkit_secure_coding':
                    dictionary_members.append(MemberVariable(member_type, member_name, member_condition, []))
                else:
                    members.append(MemberVariable(member_type, member_name, member_condition, []))
    return [serialized_types, serialized_enums, headers, using_statements, additional_forward_declarations, objc_wrapped_types]
