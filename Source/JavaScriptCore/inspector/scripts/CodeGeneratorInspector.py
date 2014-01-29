#!/usr/bin/env python
# Copyright (c) 2011 Google Inc. All rights reserved.
# Copyright (c) 2012 Intel Corporation. All rights reserved.
# Copyright (c) 2013 Apple Inc. All Rights Reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os.path
import sys
import string
import optparse
import re
try:
    import json
except ImportError:
    import simplejson as json

import CodeGeneratorInspectorStrings


DOMAIN_DEFINE_NAME_MAP = {
    "Database": "SQL_DATABASE",
    "IndexedDB": "INDEXED_DATABASE",
}


# Manually-filled map of type name replacements.
TYPE_NAME_FIX_MAP = {
    "RGBA": "Rgba",  # RGBA is reported to be conflicting with a define name in Windows CE.
    "": "Empty",
}


TYPES_WITH_RUNTIME_CAST_SET = frozenset(["Runtime.RemoteObject", "Runtime.PropertyDescriptor", "Runtime.InternalPropertyDescriptor",
                                         "Debugger.FunctionDetails", "Debugger.CallFrame",
                                         "Canvas.TraceLog", "Canvas.ResourceInfo", "Canvas.ResourceState",
                                         # This should be a temporary hack. TimelineEvent should be created via generated C++ API.
                                         "Timeline.TimelineEvent"])

TYPES_WITH_OPEN_FIELD_LIST_SET = frozenset(["Timeline.TimelineEvent",
                                            # InspectorStyleSheet not only creates this property but wants to read it and modify it.
                                            "CSS.CSSProperty",
                                            # InspectorResourceAgent needs to update mime-type.
                                            "Network.Response"])

EXACTLY_INT_SUPPORTED = False

INSPECTOR_TYPES_GENERATOR_CONFIG_MAP = {
    "JavaScript": {
        "prefix": "JS",
        "typebuilder_dependency": "",
        "export_macro": "JS_EXPORT_PRIVATE",
    },
    "Web": {
        "prefix": "Web",
        "typebuilder_dependency": "#include <inspector/InspectorJSTypeBuilders.h>",
        "export_macro": "",
    },
}

cmdline_parser = optparse.OptionParser(usage="usage: %prog [options] <Inspector.json>")
cmdline_parser.add_option("--output_h_dir")
cmdline_parser.add_option("--output_cpp_dir")
cmdline_parser.add_option("--output_js_dir")
cmdline_parser.add_option("--output_type")  # JavaScript, Web
cmdline_parser.add_option("--write_always", action="store_true")
cmdline_parser.add_option("--no_verification", action="store_true")

try:
    arg_options, arg_values = cmdline_parser.parse_args()
    if (len(arg_values) < 1):
        raise Exception("At least one plain argument expected")

    input_json_filename = arg_values[0]
    dependency_json_filenames = arg_values[1:]

    output_header_dirname = arg_options.output_h_dir
    output_cpp_dirname = arg_options.output_cpp_dir
    output_js_dirname = arg_options.output_js_dir
    output_type = arg_options.output_type

    write_always = arg_options.write_always
    verification = not arg_options.no_verification
    if not output_header_dirname:
        raise Exception("Output .h directory must be specified")
    if not output_cpp_dirname:
        raise Exception("Output .cpp directory must be specified")
    if not output_js_dirname:
        raise Exception("Output .js directory must be specified")
    if output_type not in INSPECTOR_TYPES_GENERATOR_CONFIG_MAP.keys():
        raise Exception("Unknown output type. Allowed types are: %s" % INSPECTOR_TYPES_GENERATOR_CONFIG_MAP.keys())
except Exception:
    # Work with python 2 and 3 http://docs.python.org/py3k/howto/pyporting.html
    exc = sys.exc_info()[1]
    sys.stderr.write("Failed to parse command-line arguments: %s\n\n" % exc)
    sys.stderr.write("Usage: <script> Inspector.json --output_h_dir <output_header_dir> --output_cpp_dir <output_cpp_dir> --output_js_dir <output_js_dir> [--write_always] [--no_verification]\n")
    exit(1)


def dash_to_camelcase(word):
    return ''.join(x.capitalize() or '-' for x in word.split('-'))


def fix_camel_case(name):
    refined = re.sub(r'-(\w)', lambda pat: pat.group(1).upper(), name)
    refined = to_title_case(refined)
    return re.sub(r'(?i)HTML|XML|WML|API|GC|XHR|DOM|CSS', lambda pat: pat.group(0).upper(), refined)


def to_title_case(name):
    return name[:1].upper() + name[1:]


class Capitalizer:
    @staticmethod
    def lower_camel_case_to_upper(str):
        if len(str) > 0 and str[0].islower():
            str = str[0].upper() + str[1:]
        return str

    @staticmethod
    def upper_camel_case_to_lower(str):
        pos = 0
        while pos < len(str) and str[pos].isupper():
            pos += 1
        if pos == 0:
            return str
        if pos == 1:
            return str[0].lower() + str[1:]
        if pos < len(str):
            pos -= 1
        possible_abbreviation = str[0:pos]
        if possible_abbreviation not in Capitalizer.ABBREVIATION:
            raise Exception("Unknown abbreviation %s" % possible_abbreviation)
        str = possible_abbreviation.lower() + str[pos:]
        return str

    @staticmethod
    def camel_case_to_capitalized_with_underscores(str):
        if len(str) == 0:
            return str
        output = Capitalizer.split_camel_case_(str)
        return "_".join(output).upper()

    @staticmethod
    def split_camel_case_(str):
        output = []
        pos_being = 0
        pos = 1
        has_oneletter = False
        while pos < len(str):
            if str[pos].isupper():
                output.append(str[pos_being:pos].upper())
                if pos - pos_being == 1:
                    has_oneletter = True
                pos_being = pos
            pos += 1
        output.append(str[pos_being:])
        if has_oneletter:
            array_pos = 0
            while array_pos < len(output) - 1:
                if len(output[array_pos]) == 1:
                    array_pos_end = array_pos + 1
                    while array_pos_end < len(output) and len(output[array_pos_end]) == 1:
                        array_pos_end += 1
                    if array_pos_end - array_pos > 1:
                        possible_abbreviation = "".join(output[array_pos:array_pos_end])
                        if possible_abbreviation.upper() in Capitalizer.ABBREVIATION:
                            output[array_pos:array_pos_end] = [possible_abbreviation]
                        else:
                            array_pos = array_pos_end - 1
                array_pos += 1
        return output

    ABBREVIATION = frozenset(["XHR", "DOM", "CSS"])

VALIDATOR_IFDEF_NAME = "!ASSERT_DISABLED"


class DomainNameFixes:
    @classmethod
    def get_fixed_data(cls, domain_name):
        field_name_res = Capitalizer.upper_camel_case_to_lower(domain_name) + "Agent"

        class Res(object):
            skip_js_bind = domain_name in cls.skip_js_bind_domains

            @staticmethod
            def get_guard():
                if domain_name in DOMAIN_DEFINE_NAME_MAP:
                    define_name = DOMAIN_DEFINE_NAME_MAP[domain_name]

                    class Guard:
                        @staticmethod
                        def generate_open(output):
                            output.append("#if ENABLE(%s)\n" % define_name)

                        @staticmethod
                        def generate_close(output):
                            output.append("#endif // ENABLE(%s)\n" % define_name)

                    return Guard

        return Res

    skip_js_bind_domains = set(["DOMDebugger"])


class RawTypes(object):
    @staticmethod
    def get(json_type):
        if json_type == "boolean":
            return RawTypes.Bool
        elif json_type == "string":
            return RawTypes.String
        elif json_type == "array":
            return RawTypes.Array
        elif json_type == "object":
            return RawTypes.Object
        elif json_type == "integer":
            return RawTypes.Int
        elif json_type == "number":
            return RawTypes.Number
        elif json_type == "any":
            return RawTypes.Any
        else:
            raise Exception("Unknown type: %s" % json_type)

    # For output parameter all values are passed by pointer except RefPtr-based types.
    class OutputPassModel:
        class ByPointer:
            @staticmethod
            def get_argument_prefix():
                return "&"

            @staticmethod
            def get_parameter_type_suffix():
                return "*"

        class ByReference:
            @staticmethod
            def get_argument_prefix():
                return ""

            @staticmethod
            def get_parameter_type_suffix():
                return "&"

    class BaseType(object):
        need_internal_runtime_cast_ = False

        @classmethod
        def request_raw_internal_runtime_cast(cls):
            if not cls.need_internal_runtime_cast_:
                cls.need_internal_runtime_cast_ = True

        @classmethod
        def get_raw_validator_call_text(cls):
            return "RuntimeCastHelper::assertType<Inspector::InspectorValue::Type%s>" % cls.get_validate_method_params().template_type

    class String(BaseType):
        @staticmethod
        def get_getter_name():
            return "String"

        get_setter_name = get_getter_name

        @staticmethod
        def get_c_initializer():
            return "\"\""

        @staticmethod
        def get_js_bind_type():
            return "string"

        @staticmethod
        def get_validate_method_params():
            class ValidateMethodParams:
                template_type = "String"
            return ValidateMethodParams

        @staticmethod
        def get_output_pass_model():
            return RawTypes.OutputPassModel.ByPointer

        @staticmethod
        def is_heavy_value():
            return True

        @staticmethod
        def get_array_item_raw_c_type_text():
            return "String"

        @staticmethod
        def get_raw_type_model():
            return TypeModel.String

    class Int(BaseType):
        @staticmethod
        def get_getter_name():
            return "Int"

        @staticmethod
        def get_setter_name():
            return "Number"

        @staticmethod
        def get_c_initializer():
            return "0"

        @staticmethod
        def get_js_bind_type():
            return "number"

        @classmethod
        def get_raw_validator_call_text(cls):
            return "RuntimeCastHelper::assertInt"

        @staticmethod
        def get_output_pass_model():
            return RawTypes.OutputPassModel.ByPointer

        @staticmethod
        def is_heavy_value():
            return False

        @staticmethod
        def get_array_item_raw_c_type_text():
            return "int"

        @staticmethod
        def get_raw_type_model():
            return TypeModel.Int

    class Number(BaseType):
        @staticmethod
        def get_getter_name():
            return "Double"

        @staticmethod
        def get_setter_name():
            return "Number"

        @staticmethod
        def get_c_initializer():
            return "0"

        @staticmethod
        def get_js_bind_type():
            return "number"

        @staticmethod
        def get_validate_method_params():
            class ValidateMethodParams:
                template_type = "Number"
            return ValidateMethodParams

        @staticmethod
        def get_output_pass_model():
            return RawTypes.OutputPassModel.ByPointer

        @staticmethod
        def is_heavy_value():
            return False

        @staticmethod
        def get_array_item_raw_c_type_text():
            return "double"

        @staticmethod
        def get_raw_type_model():
            return TypeModel.Number

    class Bool(BaseType):
        @staticmethod
        def get_getter_name():
            return "Boolean"

        get_setter_name = get_getter_name

        @staticmethod
        def get_c_initializer():
            return "false"

        @staticmethod
        def get_js_bind_type():
            return "boolean"

        @staticmethod
        def get_validate_method_params():
            class ValidateMethodParams:
                template_type = "Boolean"
            return ValidateMethodParams

        @staticmethod
        def get_output_pass_model():
            return RawTypes.OutputPassModel.ByPointer

        @staticmethod
        def is_heavy_value():
            return False

        @staticmethod
        def get_array_item_raw_c_type_text():
            return "bool"

        @staticmethod
        def get_raw_type_model():
            return TypeModel.Bool

    class Object(BaseType):
        @staticmethod
        def get_getter_name():
            return "Object"

        @staticmethod
        def get_setter_name():
            return "Value"

        @staticmethod
        def get_c_initializer():
            return "InspectorObject::create()"

        @staticmethod
        def get_js_bind_type():
            return "object"

        @staticmethod
        def get_output_argument_prefix():
            return ""

        @staticmethod
        def get_validate_method_params():
            class ValidateMethodParams:
                template_type = "Object"
            return ValidateMethodParams

        @staticmethod
        def get_output_pass_model():
            return RawTypes.OutputPassModel.ByReference

        @staticmethod
        def is_heavy_value():
            return True

        @staticmethod
        def get_array_item_raw_c_type_text():
            return "Inspector::InspectorObject"

        @staticmethod
        def get_raw_type_model():
            return TypeModel.Object

    class Any(BaseType):
        @staticmethod
        def get_getter_name():
            return "Value"

        get_setter_name = get_getter_name

        @staticmethod
        def get_c_initializer():
            raise Exception("Unsupported")

        @staticmethod
        def get_js_bind_type():
            raise Exception("Unsupported")

        @staticmethod
        def get_raw_validator_call_text():
            return "RuntimeCastHelper::assertAny"

        @staticmethod
        def get_output_pass_model():
            return RawTypes.OutputPassModel.ByReference

        @staticmethod
        def is_heavy_value():
            return True

        @staticmethod
        def get_array_item_raw_c_type_text():
            return "Inspector::InspectorValue"

        @staticmethod
        def get_raw_type_model():
            return TypeModel.Any

    class Array(BaseType):
        @staticmethod
        def get_getter_name():
            return "Array"

        @staticmethod
        def get_setter_name():
            return "Value"

        @staticmethod
        def get_c_initializer():
            return "InspectorArray::create()"

        @staticmethod
        def get_js_bind_type():
            return "object"

        @staticmethod
        def get_output_argument_prefix():
            return ""

        @staticmethod
        def get_validate_method_params():
            class ValidateMethodParams:
                template_type = "Array"
            return ValidateMethodParams

        @staticmethod
        def get_output_pass_model():
            return RawTypes.OutputPassModel.ByReference

        @staticmethod
        def is_heavy_value():
            return True

        @staticmethod
        def get_array_item_raw_c_type_text():
            return "Inspector::InspectorArray"

        @staticmethod
        def get_raw_type_model():
            return TypeModel.Array


def replace_right_shift(input_str):
    return input_str.replace(">>", "> >")


class CommandReturnPassModel:
    class ByReference:
        def __init__(self, var_type, set_condition):
            self.var_type = var_type
            self.set_condition = set_condition

        def get_return_var_type(self):
            return self.var_type

        @staticmethod
        def get_output_argument_prefix():
            return ""

        @staticmethod
        def get_output_to_raw_expression():
            return "%s"

        def get_output_parameter_type(self):
            return self.var_type + "&"

        def get_set_return_condition(self):
            return self.set_condition

    class ByPointer:
        def __init__(self, var_type):
            self.var_type = var_type

        def get_return_var_type(self):
            return self.var_type

        @staticmethod
        def get_output_argument_prefix():
            return "&"

        @staticmethod
        def get_output_to_raw_expression():
            return "%s"

        def get_output_parameter_type(self):
            return self.var_type + "*"

        @staticmethod
        def get_set_return_condition():
            return None

    class OptOutput:
        def __init__(self, var_type):
            self.var_type = var_type

        def get_return_var_type(self):
            return "Inspector::TypeBuilder::OptOutput<%s>" % self.var_type

        @staticmethod
        def get_output_argument_prefix():
            return "&"

        @staticmethod
        def get_output_to_raw_expression():
            return "%s.getValue()"

        def get_output_parameter_type(self):
            return "Inspector::TypeBuilder::OptOutput<%s>*" % self.var_type

        @staticmethod
        def get_set_return_condition():
            return "%s.isAssigned()"


class TypeModel:
    class RefPtrBased(object):
        def __init__(self, class_name):
            self.class_name = class_name
            self.optional = False

        def get_optional(self):
            result = TypeModel.RefPtrBased(self.class_name)
            result.optional = True
            return result

        def get_command_return_pass_model(self):
            if self.optional:
                set_condition = "%s"
            else:
                set_condition = None
            return CommandReturnPassModel.ByReference(replace_right_shift("RefPtr<%s>" % self.class_name), set_condition)

        def get_input_param_type_text(self):
            return replace_right_shift("PassRefPtr<%s>" % self.class_name)

        @staticmethod
        def get_event_setter_expression_pattern():
            return "%s"

    class Enum(object):
        def __init__(self, base_type_name):
            self.type_name = base_type_name + "::Enum"

        def get_optional(base_self):
            class EnumOptional:
                @classmethod
                def get_optional(cls):
                    return cls

                @staticmethod
                def get_command_return_pass_model():
                    return CommandReturnPassModel.OptOutput(base_self.type_name)

                @staticmethod
                def get_input_param_type_text():
                    return base_self.type_name + "*"

                @staticmethod
                def get_event_setter_expression_pattern():
                    raise Exception("TODO")
            return EnumOptional

        def get_command_return_pass_model(self):
            return CommandReturnPassModel.ByPointer(self.type_name)

        def get_input_param_type_text(self):
            return self.type_name

        @staticmethod
        def get_event_setter_expression_pattern():
            return "%s"

    class ValueType(object):
        def __init__(self, type_name, is_heavy):
            self.type_name = type_name
            self.is_heavy = is_heavy

        def get_optional(self):
            return self.ValueOptional(self)

        def get_command_return_pass_model(self):
            return CommandReturnPassModel.ByPointer(self.type_name)

        def get_input_param_type_text(self):
            if self.is_heavy:
                return "const %s&" % self.type_name
            else:
                return self.type_name

        def get_opt_output_type_(self):
            return self.type_name

        @staticmethod
        def get_event_setter_expression_pattern():
            return "%s"

        class ValueOptional:
            def __init__(self, base):
                self.base = base

            def get_optional(self):
                return self

            def get_command_return_pass_model(self):
                return CommandReturnPassModel.OptOutput(self.base.get_opt_output_type_())

            def get_input_param_type_text(self):
                return "const %s* const" % self.base.type_name

            @staticmethod
            def get_event_setter_expression_pattern():
                return "*%s"

    class ExactlyInt(ValueType):
        def __init__(self):
            TypeModel.ValueType.__init__(self, "int", False)

        def get_input_param_type_text(self):
            return "Inspector::TypeBuilder::ExactlyInt"

        def get_opt_output_type_(self):
            return "Inspector::TypeBuilder::ExactlyInt"

    @classmethod
    def init_class(cls):
        cls.Bool = cls.ValueType("bool", False)
        if EXACTLY_INT_SUPPORTED:
            cls.Int = cls.ExactlyInt()
        else:
            cls.Int = cls.ValueType("int", False)
        cls.Number = cls.ValueType("double", False)
        cls.String = cls.ValueType("String", True,)
        cls.Object = cls.RefPtrBased("Inspector::InspectorObject")
        cls.Array = cls.RefPtrBased("Inspector::InspectorArray")
        cls.Any = cls.RefPtrBased("Inspector::InspectorValue")

TypeModel.init_class()


# Collection of InspectorObject class methods that are likely to be overloaded in generated class.
# We must explicitly import all overloaded methods or they won't be available to user.
INSPECTOR_OBJECT_SETTER_NAMES = frozenset(["setValue", "setBoolean", "setNumber", "setString", "setValue", "setObject", "setArray"])


def fix_type_name(json_name):
    if json_name in TYPE_NAME_FIX_MAP:
        fixed = TYPE_NAME_FIX_MAP[json_name]

        class Result(object):
            class_name = fixed

            @staticmethod
            def output_comment(writer):
                writer.newline("// Type originally was named '%s'.\n" % json_name)
    else:

        class Result(object):
            class_name = json_name

            @staticmethod
            def output_comment(writer):
                pass

    return Result


class Writer:
    def __init__(self, output, indent):
        self.output = output
        self.indent = indent

    def newline(self, str):
        if (self.indent):
            self.output.append(self.indent)
        self.output.append(str)

    def append(self, str):
        self.output.append(str)

    def newline_multiline(self, str):
        parts = str.split('\n')
        self.newline(parts[0])
        for p in parts[1:]:
            self.output.append('\n')
            if p:
                self.newline(p)

    def append_multiline(self, str):
        parts = str.split('\n')
        self.append(parts[0])
        for p in parts[1:]:
            self.output.append('\n')
            if p:
                self.newline(p)

    def get_indent(self):
        return self.indent

    def get_indented(self, additional_indent):
        return Writer(self.output, self.indent + additional_indent)

    def insert_writer(self, additional_indent):
        new_output = []
        self.output.append(new_output)
        return Writer(new_output, self.indent + additional_indent)


class EnumConstants:
    map_ = {}
    constants_ = []

    @classmethod
    def add_constant(cls, value):
        if value in cls.map_:
            return cls.map_[value]
        else:
            pos = len(cls.map_)
            cls.map_[value] = pos
            cls.constants_.append(value)
            return pos

    @classmethod
    def get_enum_constant_code(cls):
        output = []
        for item in cls.constants_:
            output.append("    \"" + item + "\"")
        return ",\n".join(output) + "\n"


# Typebuilder code is generated in several passes: first typedefs, then other classes.
# Manual pass management is needed because we cannot have forward declarations for typedefs.
class TypeBuilderPass:
    TYPEDEF = "typedef"
    MAIN = "main"


class TypeBindings:
    @staticmethod
    def create_named_type_declaration(json_typable, context_domain_name, type_data):
        json_type = type_data.get_json_type()

        class Helper:
            is_ad_hoc = False
            full_name_prefix_for_use = "Inspector::TypeBuilder::" + context_domain_name + "::"
            full_name_prefix_for_impl = "Inspector::TypeBuilder::" + context_domain_name + "::"

            @staticmethod
            def write_doc(writer):
                if "description" in json_type:
                    writer.newline("/* ")
                    writer.append(json_type["description"])
                    writer.append(" */\n")

            @staticmethod
            def add_to_forward_listener(forward_listener):
                forward_listener.add_type_data(type_data)


        fixed_type_name = fix_type_name(json_type["id"])
        return TypeBindings.create_type_declaration_(json_typable, context_domain_name, fixed_type_name, Helper)

    @staticmethod
    def create_ad_hoc_type_declaration(json_typable, context_domain_name, ad_hoc_type_context):
        class Helper:
            is_ad_hoc = True
            full_name_prefix_for_use = ad_hoc_type_context.container_relative_name_prefix
            full_name_prefix_for_impl = ad_hoc_type_context.container_full_name_prefix

            @staticmethod
            def write_doc(writer):
                pass

            @staticmethod
            def add_to_forward_listener(forward_listener):
                pass
        fixed_type_name = ad_hoc_type_context.get_type_name_fix()
        return TypeBindings.create_type_declaration_(json_typable, context_domain_name, fixed_type_name, Helper)

    @staticmethod
    def create_type_declaration_(json_typable, context_domain_name, fixed_type_name, helper):
        if json_typable["type"] == "string":
            if "enum" in json_typable:

                class EnumBinding:
                    need_user_runtime_cast_ = False
                    need_internal_runtime_cast_ = False

                    @classmethod
                    def resolve_inner(cls, resolve_context):
                        pass

                    @classmethod
                    def request_user_runtime_cast(cls, request):
                        if request:
                            cls.need_user_runtime_cast_ = True
                            request.acknowledge()

                    @classmethod
                    def request_internal_runtime_cast(cls):
                        cls.need_internal_runtime_cast_ = True

                    @classmethod
                    def get_code_generator(enum_binding_cls):
                        #FIXME: generate ad-hoc enums too once we figure out how to better implement them in C++.
                        comment_out = helper.is_ad_hoc

                        class CodeGenerator:
                            @staticmethod
                            def generate_type_builder(writer, generate_context):
                                enum = json_typable["enum"]
                                helper.write_doc(writer)
                                enum_name = fixed_type_name.class_name
                                fixed_type_name.output_comment(writer)
                                writer.newline("struct ")
                                writer.append(enum_name)
                                writer.append(" {\n")
                                writer.newline("    enum Enum {\n")
                                for enum_item in enum:
                                    enum_pos = EnumConstants.add_constant(enum_item)

                                    item_c_name = fix_camel_case(enum_item)
                                    if item_c_name in TYPE_NAME_FIX_MAP:
                                        item_c_name = TYPE_NAME_FIX_MAP[item_c_name]
                                    writer.newline("        ")
                                    writer.append(item_c_name)
                                    writer.append(" = ")
                                    writer.append("%s" % enum_pos)
                                    writer.append(",\n")
                                writer.newline("    };\n")
                                if enum_binding_cls.need_user_runtime_cast_:
                                    raise Exception("Not yet implemented")

                                if enum_binding_cls.need_internal_runtime_cast_:
                                    writer.append("#if %s\n" % VALIDATOR_IFDEF_NAME)
                                    writer.newline("    static void assertCorrectValue(Inspector::InspectorValue* value);\n")
                                    writer.append("#endif  // %s\n" % VALIDATOR_IFDEF_NAME)

                                    validator_writer = generate_context.validator_writer

                                    domain_fixes = DomainNameFixes.get_fixed_data(context_domain_name)
                                    domain_guard = domain_fixes.get_guard()
                                    if domain_guard:
                                        domain_guard.generate_open(validator_writer)

                                    validator_writer.newline("void %s%s::assertCorrectValue(Inspector::InspectorValue* value)\n" % (helper.full_name_prefix_for_impl, enum_name))
                                    validator_writer.newline("{\n")
                                    validator_writer.newline("    WTF::String s;\n")
                                    validator_writer.newline("    bool cast_res = value->asString(&s);\n")
                                    validator_writer.newline("    ASSERT(cast_res);\n")
                                    if len(enum) > 0:
                                        condition_list = []
                                        for enum_item in enum:
                                            enum_pos = EnumConstants.add_constant(enum_item)
                                            condition_list.append("s == \"%s\"" % enum_item)
                                        validator_writer.newline("    ASSERT(%s);\n" % " || ".join(condition_list))
                                    validator_writer.newline("}\n")

                                    if domain_guard:
                                        domain_guard.generate_close(validator_writer)

                                    validator_writer.newline("\n\n")

                                writer.newline("}; // struct ")
                                writer.append(enum_name)
                                writer.append("\n")

                            @staticmethod
                            def register_use(forward_listener):
                                pass

                            @staticmethod
                            def get_generate_pass_id():
                                return TypeBuilderPass.MAIN

                        return CodeGenerator

                    @classmethod
                    def get_validator_call_text(cls):
                        return helper.full_name_prefix_for_use + fixed_type_name.class_name + "::assertCorrectValue"

                    @classmethod
                    def get_array_item_c_type_text(cls):
                        return helper.full_name_prefix_for_use + fixed_type_name.class_name + "::Enum"

                    @staticmethod
                    def get_setter_value_expression_pattern():
                        return "Inspector::TypeBuilder::get%sEnumConstantValue(%s)"

                    @staticmethod
                    def reduce_to_raw_type():
                        return RawTypes.String

                    @staticmethod
                    def get_type_model():
                        return TypeModel.Enum(helper.full_name_prefix_for_use + fixed_type_name.class_name)

                return EnumBinding
            else:
                if helper.is_ad_hoc:

                    class PlainString:
                        @classmethod
                        def resolve_inner(cls, resolve_context):
                            pass

                        @staticmethod
                        def request_user_runtime_cast(request):
                            raise Exception("Unsupported")

                        @staticmethod
                        def request_internal_runtime_cast():
                            pass

                        @staticmethod
                        def get_code_generator():
                            return None

                        @classmethod
                        def get_validator_call_text(cls):
                            return RawTypes.String.get_raw_validator_call_text()

                        @staticmethod
                        def reduce_to_raw_type():
                            return RawTypes.String

                        @staticmethod
                        def get_type_model():
                            return TypeModel.String

                        @staticmethod
                        def get_setter_value_expression_pattern():
                            return None

                        @classmethod
                        def get_array_item_c_type_text(cls):
                            return cls.reduce_to_raw_type().get_array_item_raw_c_type_text()

                    return PlainString

                else:

                    class TypedefString:
                        @classmethod
                        def resolve_inner(cls, resolve_context):
                            pass

                        @staticmethod
                        def request_user_runtime_cast(request):
                            raise Exception("Unsupported")

                        @staticmethod
                        def request_internal_runtime_cast():
                            RawTypes.String.request_raw_internal_runtime_cast()

                        @staticmethod
                        def get_code_generator():
                            class CodeGenerator:
                                @staticmethod
                                def generate_type_builder(writer, generate_context):
                                    helper.write_doc(writer)
                                    fixed_type_name.output_comment(writer)
                                    writer.newline("typedef String ")
                                    writer.append(fixed_type_name.class_name)
                                    writer.append(";\n\n")

                                @staticmethod
                                def register_use(forward_listener):
                                    pass

                                @staticmethod
                                def get_generate_pass_id():
                                    return TypeBuilderPass.TYPEDEF

                            return CodeGenerator

                        @classmethod
                        def get_validator_call_text(cls):
                            return RawTypes.String.get_raw_validator_call_text()

                        @staticmethod
                        def reduce_to_raw_type():
                            return RawTypes.String

                        @staticmethod
                        def get_type_model():
                            return TypeModel.ValueType("%s%s" % (helper.full_name_prefix_for_use, fixed_type_name.class_name), True)

                        @staticmethod
                        def get_setter_value_expression_pattern():
                            return None

                        @classmethod
                        def get_array_item_c_type_text(cls):
                            return "const %s%s&" % (helper.full_name_prefix_for_use, fixed_type_name.class_name)

                    return TypedefString

        elif json_typable["type"] == "integer":
                if helper.is_ad_hoc:

                    class PlainInteger:
                        @classmethod
                        def resolve_inner(cls, resolve_context):
                            pass

                        @staticmethod
                        def request_user_runtime_cast(request):
                            raise Exception("Unsupported")

                        @staticmethod
                        def request_internal_runtime_cast():
                            pass

                        @staticmethod
                        def get_code_generator():
                            return None

                        @classmethod
                        def get_validator_call_text(cls):
                            return RawTypes.Int.get_raw_validator_call_text()

                        @staticmethod
                        def reduce_to_raw_type():
                            return RawTypes.Int

                        @staticmethod
                        def get_type_model():
                            return TypeModel.Int

                        @staticmethod
                        def get_setter_value_expression_pattern():
                            return None

                        @classmethod
                        def get_array_item_c_type_text(cls):
                            return cls.reduce_to_raw_type().get_array_item_raw_c_type_text()

                    return PlainInteger

                else:

                    class TypedefInteger:
                        @classmethod
                        def resolve_inner(cls, resolve_context):
                            pass

                        @staticmethod
                        def request_user_runtime_cast(request):
                            raise Exception("Unsupported")

                        @staticmethod
                        def request_internal_runtime_cast():
                            RawTypes.Int.request_raw_internal_runtime_cast()

                        @staticmethod
                        def get_code_generator():
                            class CodeGenerator:
                                @staticmethod
                                def generate_type_builder(writer, generate_context):
                                    helper.write_doc(writer)
                                    fixed_type_name.output_comment(writer)
                                    writer.newline("typedef int ")
                                    writer.append(fixed_type_name.class_name)
                                    writer.append(";\n\n")

                                @staticmethod
                                def register_use(forward_listener):
                                    pass

                                @staticmethod
                                def get_generate_pass_id():
                                    return TypeBuilderPass.TYPEDEF

                            return CodeGenerator

                        @classmethod
                        def get_validator_call_text(cls):
                            return RawTypes.Int.get_raw_validator_call_text()

                        @staticmethod
                        def reduce_to_raw_type():
                            return RawTypes.Int

                        @staticmethod
                        def get_type_model():
                            return TypeModel.Int

                        @staticmethod
                        def get_setter_value_expression_pattern():
                            return None

                        @classmethod
                        def get_array_item_c_type_text(cls):
                            return helper.full_name_prefix_for_use + fixed_type_name.class_name

                    return TypedefInteger

        elif json_typable["type"] == "object":
            if "properties" in json_typable:

                class ClassBinding:
                    resolve_data_ = None
                    need_user_runtime_cast_ = False
                    need_internal_runtime_cast_ = False

                    @classmethod
                    def resolve_inner(cls, resolve_context):
                        if cls.resolve_data_:
                            return

                        properties = json_typable["properties"]
                        main = []
                        optional = []

                        ad_hoc_type_list = []

                        for prop in properties:
                            prop_name = prop["name"]
                            ad_hoc_type_context = cls.AdHocTypeContextImpl(prop_name, fixed_type_name.class_name, resolve_context, ad_hoc_type_list, helper.full_name_prefix_for_impl)
                            binding = resolve_param_type(prop, context_domain_name, ad_hoc_type_context)

                            code_generator = binding.get_code_generator()
                            if code_generator:
                                code_generator.register_use(resolve_context.forward_listener)

                            class PropertyData:
                                param_type_binding = binding
                                p = prop

                            if prop.get("optional"):
                                optional.append(PropertyData)
                            else:
                                main.append(PropertyData)

                        class ResolveData:
                            main_properties = main
                            optional_properties = optional
                            ad_hoc_types = ad_hoc_type_list

                        cls.resolve_data_ = ResolveData

                        for ad_hoc in ad_hoc_type_list:
                            ad_hoc.resolve_inner(resolve_context)

                    @classmethod
                    def request_user_runtime_cast(cls, request):
                        if not request:
                            return
                        cls.need_user_runtime_cast_ = True
                        request.acknowledge()
                        cls.request_internal_runtime_cast()

                    @classmethod
                    def request_internal_runtime_cast(cls):
                        if cls.need_internal_runtime_cast_:
                            return
                        cls.need_internal_runtime_cast_ = True
                        for p in cls.resolve_data_.main_properties:
                            p.param_type_binding.request_internal_runtime_cast()
                        for p in cls.resolve_data_.optional_properties:
                            p.param_type_binding.request_internal_runtime_cast()

                    @classmethod
                    def get_code_generator(class_binding_cls):
                        class CodeGenerator:
                            @classmethod
                            def generate_type_builder(cls, writer, generate_context):
                                resolve_data = class_binding_cls.resolve_data_
                                helper.write_doc(writer)
                                class_name = fixed_type_name.class_name

                                is_open_type = (context_domain_name + "." + class_name) in TYPES_WITH_OPEN_FIELD_LIST_SET

                                fixed_type_name.output_comment(writer)
                                writer.newline("class ")
                                writer.append(class_name)
                                writer.append(" : public ")
                                if is_open_type:
                                    writer.append("Inspector::InspectorObject")
                                else:
                                    writer.append("Inspector::InspectorObjectBase")
                                writer.append(" {\n")
                                writer.newline("public:\n")
                                ad_hoc_type_writer = writer.insert_writer("    ")

                                for ad_hoc_type in resolve_data.ad_hoc_types:
                                    code_generator = ad_hoc_type.get_code_generator()
                                    if code_generator:
                                        code_generator.generate_type_builder(ad_hoc_type_writer, generate_context)

                                writer.newline_multiline(
"""    enum {
        NoFieldsSet = 0,
""")

                                state_enum_items = []
                                if len(resolve_data.main_properties) > 0:
                                    pos = 0
                                    for prop_data in resolve_data.main_properties:
                                        item_name = Capitalizer.lower_camel_case_to_upper(prop_data.p["name"]) + "Set"
                                        state_enum_items.append(item_name)
                                        writer.newline("        %s = 1 << %s,\n" % (item_name, pos))
                                        pos += 1
                                    all_fields_set_value = "(" + (" | ".join(state_enum_items)) + ")"
                                else:
                                    all_fields_set_value = "0"

                                writer.newline_multiline(CodeGeneratorInspectorStrings.class_binding_builder_part_1
                                                         % (all_fields_set_value, class_name, class_name))

                                pos = 0
                                for prop_data in resolve_data.main_properties:
                                    prop_name = prop_data.p["name"]

                                    param_type_binding = prop_data.param_type_binding
                                    param_raw_type = param_type_binding.reduce_to_raw_type()

                                    writer.newline_multiline(CodeGeneratorInspectorStrings.class_binding_builder_part_2
                                        % (state_enum_items[pos],
                                           Capitalizer.lower_camel_case_to_upper(prop_name),
                                           param_type_binding.get_type_model().get_input_param_type_text(),
                                           state_enum_items[pos], prop_name,
                                           param_raw_type.get_setter_name(), prop_name,
                                           format_setter_value_expression(param_type_binding, "value"),
                                           state_enum_items[pos]))

                                    pos += 1

                                writer.newline_multiline(CodeGeneratorInspectorStrings.class_binding_builder_part_3
                                                         % (class_name, class_name, class_name, class_name, class_name))

                                writer.newline("    /*\n")
                                writer.newline("     * Synthetic constructor:\n")
                                writer.newline("     * RefPtr<%s> result = %s::create()" % (class_name, class_name))
                                for prop_data in resolve_data.main_properties:
                                    writer.append_multiline("\n     *     .set%s(...)" % Capitalizer.lower_camel_case_to_upper(prop_data.p["name"]))
                                writer.append_multiline(";\n     */\n")

                                writer.newline_multiline(CodeGeneratorInspectorStrings.class_binding_builder_part_4)

                                writer.newline("    typedef Inspector::TypeBuilder::StructItemTraits ItemTraits;\n")

                                for prop_data in resolve_data.optional_properties:
                                    prop_name = prop_data.p["name"]
                                    param_type_binding = prop_data.param_type_binding
                                    setter_name = "set%s" % Capitalizer.lower_camel_case_to_upper(prop_name)

                                    writer.append_multiline("\n    void %s" % setter_name)
                                    writer.append("(%s value)\n" % param_type_binding.get_type_model().get_input_param_type_text())
                                    writer.newline("    {\n")
                                    writer.newline("        this->set%s(ASCIILiteral(\"%s\"), %s);\n"
                                        % (param_type_binding.reduce_to_raw_type().get_setter_name(), prop_data.p["name"],
                                           format_setter_value_expression(param_type_binding, "value")))
                                    writer.newline("    }\n")


                                    if setter_name in INSPECTOR_OBJECT_SETTER_NAMES:
                                        writer.newline("    using Inspector::InspectorObjectBase::%s;\n\n" % setter_name)

                                if class_binding_cls.need_user_runtime_cast_:
                                    writer.newline("    static PassRefPtr<%s> runtimeCast(PassRefPtr<Inspector::InspectorValue> value)\n" % class_name)
                                    writer.newline("    {\n")
                                    writer.newline("        RefPtr<Inspector::InspectorObject> object;\n")
                                    writer.newline("        bool castRes = value->asObject(&object);\n")
                                    writer.newline("        ASSERT_UNUSED(castRes, castRes);\n")
                                    writer.append("#if %s\n" % VALIDATOR_IFDEF_NAME)
                                    writer.newline("        assertCorrectValue(object.get());\n")
                                    writer.append("#endif  // %s\n" % VALIDATOR_IFDEF_NAME)
                                    writer.newline("        COMPILE_ASSERT(sizeof(%s) == sizeof(Inspector::InspectorObjectBase), type_cast_problem);\n" % class_name)
                                    writer.newline("        return static_cast<%s*>(static_cast<Inspector::InspectorObjectBase*>(object.get()));\n" % class_name)
                                    writer.newline("    }\n")
                                    writer.append("\n")

                                if class_binding_cls.need_internal_runtime_cast_:
                                    writer.append("#if %s\n" % VALIDATOR_IFDEF_NAME)
                                    writer.newline("    static %s void assertCorrectValue(Inspector::InspectorValue* value);\n" % INSPECTOR_TYPES_GENERATOR_CONFIG_MAP[output_type]["export_macro"])
                                    writer.append("#endif  // %s\n" % VALIDATOR_IFDEF_NAME)

                                    closed_field_set = (context_domain_name + "." + class_name) not in TYPES_WITH_OPEN_FIELD_LIST_SET

                                    validator_writer = generate_context.validator_writer

                                    domain_fixes = DomainNameFixes.get_fixed_data(context_domain_name)
                                    domain_guard = domain_fixes.get_guard()
                                    if domain_guard:
                                        domain_guard.generate_open(validator_writer)

                                    validator_writer.newline("void %s%s::assertCorrectValue(Inspector::InspectorValue* value)\n" % (helper.full_name_prefix_for_impl, class_name))
                                    validator_writer.newline("{\n")
                                    validator_writer.newline("    RefPtr<InspectorObject> object;\n")
                                    validator_writer.newline("    bool castRes = value->asObject(&object);\n")
                                    validator_writer.newline("    ASSERT_UNUSED(castRes, castRes);\n")
                                    for prop_data in resolve_data.main_properties:
                                        validator_writer.newline("    {\n")
                                        it_name = "%sPos" % prop_data.p["name"]
                                        validator_writer.newline("        InspectorObject::iterator %s;\n" % it_name)
                                        validator_writer.newline("        %s = object->find(\"%s\");\n" % (it_name, prop_data.p["name"]))
                                        validator_writer.newline("        ASSERT(%s != object->end());\n" % it_name)
                                        validator_writer.newline("        %s(%s->value.get());\n" % (prop_data.param_type_binding.get_validator_call_text(), it_name))
                                        validator_writer.newline("    }\n")

                                    if closed_field_set:
                                        validator_writer.newline("    int foundPropertiesCount = %s;\n" % len(resolve_data.main_properties))

                                    for prop_data in resolve_data.optional_properties:
                                        validator_writer.newline("    {\n")
                                        it_name = "%sPos" % prop_data.p["name"]
                                        validator_writer.newline("        InspectorObject::iterator %s;\n" % it_name)
                                        validator_writer.newline("        %s = object->find(\"%s\");\n" % (it_name, prop_data.p["name"]))
                                        validator_writer.newline("        if (%s != object->end()) {\n" % it_name)
                                        validator_writer.newline("            %s(%s->value.get());\n" % (prop_data.param_type_binding.get_validator_call_text(), it_name))
                                        if closed_field_set:
                                            validator_writer.newline("            ++foundPropertiesCount;\n")
                                        validator_writer.newline("        }\n")
                                        validator_writer.newline("    }\n")

                                    if closed_field_set:
                                        validator_writer.newline("    if (foundPropertiesCount != object->size())\n")
                                        validator_writer.newline("        FATAL(\"Unexpected properties in object: %s\\n\", object->toJSONString().ascii().data());\n")
                                    validator_writer.newline("}\n")

                                    if domain_guard:
                                        domain_guard.generate_close(validator_writer)

                                    validator_writer.newline("\n\n")

                                if is_open_type:
                                    cpp_writer = generate_context.cpp_writer
                                    writer.append("\n")
                                    writer.newline("    // Property names for type generated as open.\n")
                                    for prop_data in resolve_data.main_properties + resolve_data.optional_properties:
                                        prop_name = prop_data.p["name"]
                                        prop_field_name = Capitalizer.lower_camel_case_to_upper(prop_name)
                                        writer.newline("    static const char* %s;\n" % (prop_field_name))
                                        cpp_writer.newline("const char* %s%s::%s = \"%s\";\n" % (helper.full_name_prefix_for_impl, class_name, prop_field_name, prop_name))


                                writer.newline("};\n\n")

                            @staticmethod
                            def generate_forward_declaration(writer):
                                class_name = fixed_type_name.class_name
                                writer.newline("class ")
                                writer.append(class_name)
                                writer.append(";\n")

                            @staticmethod
                            def register_use(forward_listener):
                                helper.add_to_forward_listener(forward_listener)

                            @staticmethod
                            def get_generate_pass_id():
                                return TypeBuilderPass.MAIN

                        return CodeGenerator

                    @staticmethod
                    def get_validator_call_text():
                        return helper.full_name_prefix_for_use + fixed_type_name.class_name + "::assertCorrectValue"

                    @classmethod
                    def get_array_item_c_type_text(cls):
                        return helper.full_name_prefix_for_use + fixed_type_name.class_name

                    @staticmethod
                    def get_setter_value_expression_pattern():
                        return None

                    @staticmethod
                    def reduce_to_raw_type():
                        return RawTypes.Object

                    @staticmethod
                    def get_type_model():
                        return TypeModel.RefPtrBased(helper.full_name_prefix_for_use + fixed_type_name.class_name)

                    class AdHocTypeContextImpl:
                        def __init__(self, property_name, class_name, resolve_context, ad_hoc_type_list, parent_full_name_prefix):
                            self.property_name = property_name
                            self.class_name = class_name
                            self.resolve_context = resolve_context
                            self.ad_hoc_type_list = ad_hoc_type_list
                            self.container_full_name_prefix = parent_full_name_prefix + class_name + "::"
                            self.container_relative_name_prefix = ""

                        def get_type_name_fix(self):
                            class NameFix:
                                class_name = Capitalizer.lower_camel_case_to_upper(self.property_name)

                                @staticmethod
                                def output_comment(writer):
                                    writer.newline("// Named after property name '%s' while generating %s.\n" % (self.property_name, self.class_name))

                            return NameFix

                        def add_type(self, binding):
                            self.ad_hoc_type_list.append(binding)

                return ClassBinding
            else:

                class PlainObjectBinding:
                    @classmethod
                    def resolve_inner(cls, resolve_context):
                        pass

                    @staticmethod
                    def request_user_runtime_cast(request):
                        pass

                    @staticmethod
                    def request_internal_runtime_cast():
                        RawTypes.Object.request_raw_internal_runtime_cast()

                    @staticmethod
                    def get_code_generator():
                        pass

                    @staticmethod
                    def get_validator_call_text():
                        return "RuntimeCastHelper::assertType<InspectorValue::TypeObject>"

                    @classmethod
                    def get_array_item_c_type_text(cls):
                        return cls.reduce_to_raw_type().get_array_item_raw_c_type_text()

                    @staticmethod
                    def get_setter_value_expression_pattern():
                        return None

                    @staticmethod
                    def reduce_to_raw_type():
                        return RawTypes.Object

                    @staticmethod
                    def get_type_model():
                        return TypeModel.Object

                return PlainObjectBinding
        elif json_typable["type"] == "array":
            if "items" in json_typable:

                ad_hoc_types = []

                class AdHocTypeContext:
                    container_full_name_prefix = "<not yet defined>"
                    container_relative_name_prefix = ""

                    @staticmethod
                    def get_type_name_fix():
                        return fixed_type_name

                    @staticmethod
                    def add_type(binding):
                        ad_hoc_types.append(binding)

                item_binding = resolve_param_type(json_typable["items"], context_domain_name, AdHocTypeContext)

                class ArrayBinding:
                    resolve_data_ = None
                    need_internal_runtime_cast_ = False

                    @classmethod
                    def resolve_inner(cls, resolve_context):
                        if cls.resolve_data_:
                            return

                        class ResolveData:
                            item_type_binding = item_binding
                            ad_hoc_type_list = ad_hoc_types

                        cls.resolve_data_ = ResolveData

                        for t in ad_hoc_types:
                            t.resolve_inner(resolve_context)

                    @classmethod
                    def request_user_runtime_cast(cls, request):
                        raise Exception("Not implemented yet")

                    @classmethod
                    def request_internal_runtime_cast(cls):
                        if cls.need_internal_runtime_cast_:
                            return
                        cls.need_internal_runtime_cast_ = True
                        cls.resolve_data_.item_type_binding.request_internal_runtime_cast()

                    @classmethod
                    def get_code_generator(array_binding_cls):

                        class CodeGenerator:
                            @staticmethod
                            def generate_type_builder(writer, generate_context):
                                ad_hoc_type_writer = writer

                                resolve_data = array_binding_cls.resolve_data_

                                for ad_hoc_type in resolve_data.ad_hoc_type_list:
                                    code_generator = ad_hoc_type.get_code_generator()
                                    if code_generator:
                                        code_generator.generate_type_builder(ad_hoc_type_writer, generate_context)

                            @staticmethod
                            def generate_forward_declaration(writer):
                                pass

                            @staticmethod
                            def register_use(forward_listener):
                                item_code_generator = item_binding.get_code_generator()
                                if item_code_generator:
                                    item_code_generator.register_use(forward_listener)

                            @staticmethod
                            def get_generate_pass_id():
                                return TypeBuilderPass.MAIN

                        return CodeGenerator

                    @classmethod
                    def get_validator_call_text(cls):
                        return cls.get_array_item_c_type_text() + "::assertCorrectValue"

                    @classmethod
                    def get_array_item_c_type_text(cls):
                        return replace_right_shift("Inspector::TypeBuilder::Array<%s>" % cls.resolve_data_.item_type_binding.get_array_item_c_type_text())

                    @staticmethod
                    def get_setter_value_expression_pattern():
                        return None

                    @staticmethod
                    def reduce_to_raw_type():
                        return RawTypes.Array

                    @classmethod
                    def get_type_model(cls):
                        return TypeModel.RefPtrBased(cls.get_array_item_c_type_text())

                return ArrayBinding
            else:
                # Fall-through to raw type.
                pass

        raw_type = RawTypes.get(json_typable["type"])

        return RawTypeBinding(raw_type)


class RawTypeBinding:
    def __init__(self, raw_type):
        self.raw_type_ = raw_type

    def resolve_inner(self, resolve_context):
        pass

    def request_user_runtime_cast(self, request):
        raise Exception("Unsupported")

    def request_internal_runtime_cast(self):
        self.raw_type_.request_raw_internal_runtime_cast()

    def get_code_generator(self):
        return None

    def get_validator_call_text(self):
        return self.raw_type_.get_raw_validator_call_text()

    def get_array_item_c_type_text(self):
        return self.raw_type_.get_array_item_raw_c_type_text()

    def get_setter_value_expression_pattern(self):
        return None

    def reduce_to_raw_type(self):
        return self.raw_type_

    def get_type_model(self):
        return self.raw_type_.get_raw_type_model()


class TypeData(object):
    def __init__(self, json_type, json_domain, domain_data):
        self.json_type_ = json_type
        self.json_domain_ = json_domain
        self.domain_data_ = domain_data

        if "type" not in json_type:
            raise Exception("Unknown type")

        json_type_name = json_type["type"]
        self.raw_type_ = RawTypes.get(json_type_name)
        self.binding_being_resolved_ = False
        self.binding_ = None

    def get_raw_type(self):
        return self.raw_type_

    def get_binding(self):
        if not self.binding_:
            if self.binding_being_resolved_:
                raise Exception("Type %s is already being resolved" % self.json_type_["type"])
            # Resolve only lazily, because resolving one named type may require resolving some other named type.
            self.binding_being_resolved_ = True
            try:
                self.binding_ = TypeBindings.create_named_type_declaration(self.json_type_, self.json_domain_["domain"], self)
            finally:
                self.binding_being_resolved_ = False

        return self.binding_

    def get_json_type(self):
        return self.json_type_

    def get_name(self):
        return self.json_type_["id"]

    def get_domain_name(self):
        return self.json_domain_["domain"]


class DomainData:
    def __init__(self, json_domain):
        self.json_domain = json_domain
        self.types_ = []

    def add_type(self, type_data):
        self.types_.append(type_data)

    def name(self):
        return self.json_domain["domain"]

    def types(self):
        return self.types_


class TypeMap:
    def __init__(self, api, dependency_api):
        self.map_ = {}
        self.domains_ = []
        self.domains_to_generate_ = []
        for json_domain in api["domains"]:
            self.add_domain(json_domain, True)
        for json_domain in dependency_api["domains"]:
            self.add_domain(json_domain, False)

    def add_domain(self, json_domain, should_generate):
        domain_name = json_domain["domain"]

        domain_map = {}
        self.map_[domain_name] = domain_map

        domain_data = DomainData(json_domain)
        self.domains_.append(domain_data)

        if should_generate:
            # FIXME: The order of types should not matter. The generated code should work regardless of the order of types.
            if domain_name == "Page":
                self.domains_to_generate_.insert(0, domain_data)
            else:
                self.domains_to_generate_.append(domain_data)

        if "types" in json_domain:
            for json_type in json_domain["types"]:
                type_name = json_type["id"]
                type_data = TypeData(json_type, json_domain, domain_data)
                domain_map[type_name] = type_data
                domain_data.add_type(type_data)

    def domains(self):
        return self.domains_

    def domains_to_generate(self):
        return self.domains_to_generate_

    def get(self, domain_name, type_name):
        return self.map_[domain_name][type_name]


def resolve_param_type(json_parameter, scope_domain_name, ad_hoc_type_context):
    if "$ref" in json_parameter:
        json_ref = json_parameter["$ref"]
        type_data = get_ref_data(json_ref, scope_domain_name)
        return type_data.get_binding()
    elif "type" in json_parameter:
        result = TypeBindings.create_ad_hoc_type_declaration(json_parameter, scope_domain_name, ad_hoc_type_context)
        ad_hoc_type_context.add_type(result)
        return result
    else:
        raise Exception("Unknown type")


def resolve_param_raw_type(json_parameter, scope_domain_name):
    if "$ref" in json_parameter:
        json_ref = json_parameter["$ref"]
        type_data = get_ref_data(json_ref, scope_domain_name)
        return type_data.get_raw_type()
    elif "type" in json_parameter:
        json_type = json_parameter["type"]
        return RawTypes.get(json_type)
    else:
        raise Exception("Unknown type")


def get_ref_data(json_ref, scope_domain_name):
    dot_pos = json_ref.find(".")
    if dot_pos == -1:
        domain_name = scope_domain_name
        type_name = json_ref
    else:
        domain_name = json_ref[:dot_pos]
        type_name = json_ref[dot_pos + 1:]

    return type_map.get(domain_name, type_name)


input_file = open(input_json_filename, "r")
json_string = input_file.read()
json_api = json.loads(json_string)
input_file.close()
if not "domains" in json_api:
    json_api = {"domains": [json_api]}

dependency_api = {"domains": []}
for dependency_json_filename in dependency_json_filenames:
    dependency_input_file = open(dependency_json_filename, "r")
    dependency_json_string = dependency_input_file.read()
    dependency_json_api = json.loads(dependency_json_string)
    dependency_input_file.close()
    if not "domains" in dependency_json_api:
        dependency_json_api = {"domains": [dependency_json_api]}
    dependency_api["domains"] += dependency_json_api["domains"]


class Templates:
    def get_this_script_path_(absolute_path):
        absolute_path = os.path.abspath(absolute_path)
        components = []

        def fill_recursive(path_part, depth):
            if depth <= 0 or path_part == '/':
                return
            fill_recursive(os.path.dirname(path_part), depth - 1)
            components.append(os.path.basename(path_part))

        # Typical path is /Source/WebCore/inspector/CodeGeneratorInspector.py
        # Let's take 4 components from the real path then.
        fill_recursive(absolute_path, 4)

        return "/".join(components)

    file_header_ = ("// File is generated by %s\n\n" % get_this_script_path_(sys.argv[0]) +
"""// Copyright (c) 2013 Apple Inc. All Rights Reserved.
// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
""")

    frontend_domain_class = string.Template(CodeGeneratorInspectorStrings.frontend_domain_class)
    backend_dispatcher_constructor = string.Template(CodeGeneratorInspectorStrings.backend_dispatcher_constructor)
    backend_dispatcher_dispatch_method = string.Template(CodeGeneratorInspectorStrings.backend_dispatcher_dispatch_method)
    backend_dispatcher_dispatch_method_simple = string.Template(CodeGeneratorInspectorStrings.backend_dispatcher_dispatch_method_simple)
    backend_method = string.Template(CodeGeneratorInspectorStrings.backend_method)
    frontend_method = string.Template(CodeGeneratorInspectorStrings.frontend_method)
    callback_method = string.Template(CodeGeneratorInspectorStrings.callback_method)
    frontend_h = string.Template(file_header_ + CodeGeneratorInspectorStrings.frontend_h)
    backend_h = string.Template(file_header_ + CodeGeneratorInspectorStrings.backend_h)
    backend_cpp = string.Template(file_header_ + CodeGeneratorInspectorStrings.backend_cpp)
    frontend_cpp = string.Template(file_header_ + CodeGeneratorInspectorStrings.frontend_cpp)
    typebuilder_h = string.Template(file_header_ + CodeGeneratorInspectorStrings.typebuilder_h)
    typebuilder_cpp = string.Template(file_header_ + CodeGeneratorInspectorStrings.typebuilder_cpp)
    backend_js = string.Template(file_header_ + CodeGeneratorInspectorStrings.backend_js)
    param_container_access_code = CodeGeneratorInspectorStrings.param_container_access_code





type_map = TypeMap(json_api, dependency_api)


class NeedRuntimeCastRequest:
    def __init__(self):
        self.ack_ = None

    def acknowledge(self):
        self.ack_ = True

    def is_acknowledged(self):
        return self.ack_


def resolve_all_types():
    runtime_cast_generate_requests = {}
    for type_name in TYPES_WITH_RUNTIME_CAST_SET:
        runtime_cast_generate_requests[type_name] = NeedRuntimeCastRequest()

    class ForwardListener:
        type_data_set = set()
        already_declared_set = set()

        @classmethod
        def add_type_data(cls, type_data):
            if type_data not in cls.already_declared_set:
                cls.type_data_set.add(type_data)

    class ResolveContext:
        forward_listener = ForwardListener

    for domain_data in type_map.domains():
        for type_data in domain_data.types():
            binding = type_data.get_binding()
            binding.resolve_inner(ResolveContext)
            # Do not generate forwards for this type any longer.
            ForwardListener.already_declared_set.add(type_data)

    for domain_data in type_map.domains():
        for type_data in domain_data.types():
            full_type_name = "%s.%s" % (type_data.get_domain_name(), type_data.get_name())
            request = runtime_cast_generate_requests.pop(full_type_name, None)
            binding = type_data.get_binding()
            if request:
                binding.request_user_runtime_cast(request)

            if request and not request.is_acknowledged():
                raise Exception("Failed to generate runtimeCast in " + full_type_name)

    # FIXME: This assumes all the domains are processed at once. Change this verification
    # to only verify runtime casts for the domains being generated.
    # if verification:
    #     for full_type_name in runtime_cast_generate_requests:
    #         raise Exception("Failed to generate runtimeCast. Type " + full_type_name + " not found")

    return ForwardListener


global_forward_listener = resolve_all_types()


def get_annotated_type_text(raw_type, annotated_type):
    if annotated_type != raw_type:
        return "/*%s*/ %s" % (annotated_type, raw_type)
    else:
        return raw_type


def format_setter_value_expression(param_type_binding, value_ref):
    pattern = param_type_binding.get_setter_value_expression_pattern()
    if pattern:
        return pattern % (INSPECTOR_TYPES_GENERATOR_CONFIG_MAP[output_type]["prefix"], value_ref)
    else:
        return value_ref


class Generator:
    frontend_domain_class_lines = []

    backend_method_implementation_list = []
    frontend_method_list = []
    backend_js_domain_initializer_list = []

    backend_handler_interface_list = []
    backend_handler_implementation_list = []
    backend_dispatcher_interface_list = []
    type_builder_fragments = []
    type_builder_forwards = []
    validator_impl_list = []
    type_builder_impl_list = []


    @staticmethod
    def go():
        Generator.process_types(type_map)

        first_cycle_guardable_list_list = [
            Generator.backend_method_implementation_list,
            Generator.backend_handler_interface_list,
            Generator.backend_handler_implementation_list,
            Generator.backend_dispatcher_interface_list]

        for json_domain in json_api["domains"]:
            domain_name = json_domain["domain"]
            domain_name_lower = domain_name.lower()

            domain_fixes = DomainNameFixes.get_fixed_data(domain_name)

            domain_guard = domain_fixes.get_guard()

            if domain_guard:
                for l in first_cycle_guardable_list_list:
                    domain_guard.generate_open(l)

            frontend_method_declaration_lines = []

            if ("commands" in json_domain or "events" in json_domain):
                Generator.backend_js_domain_initializer_list.append("// %s.\n" % domain_name)
                if not domain_fixes.skip_js_bind:
                    Generator.backend_js_domain_initializer_list.append("InspectorBackend.register%sDispatcher = InspectorBackend.registerDomainDispatcher.bind(InspectorBackend, \"%s\");\n" % (domain_name, domain_name))

            if "types" in json_domain:
                for json_type in json_domain["types"]:
                    if "type" in json_type and json_type["type"] == "string" and "enum" in json_type:
                        enum_name = "%s.%s" % (domain_name, json_type["id"])
                        Generator.process_enum(json_type, enum_name)
                    elif json_type["type"] == "object":
                        if "properties" in json_type:
                            for json_property in json_type["properties"]:
                                if "type" in json_property and json_property["type"] == "string" and "enum" in json_property:
                                    enum_name = "%s.%s%s" % (domain_name, json_type["id"], to_title_case(json_property["name"]))
                                    Generator.process_enum(json_property, enum_name)

            if "events" in json_domain:
                if domain_guard:
                    domain_guard.generate_open(Generator.frontend_method_list)
                    domain_guard.generate_open(Generator.frontend_domain_class_lines)

                for json_event in json_domain["events"]:
                    Generator.process_event(json_event, domain_name, frontend_method_declaration_lines)

                Generator.frontend_domain_class_lines.append(Templates.frontend_domain_class.substitute(None,
                    exportMacro=INSPECTOR_TYPES_GENERATOR_CONFIG_MAP[output_type]["export_macro"],
                    domainClassName="Inspector%sFrontendDispatcher" % domain_name,
                    frontendDomainMethodDeclarations="".join(flatten_list(frontend_method_declaration_lines))))

                if domain_guard:
                    domain_guard.generate_close(Generator.frontend_method_list)
                    domain_guard.generate_close(Generator.frontend_domain_class_lines)

            dispatcher_name = "Inspector" + Capitalizer.lower_camel_case_to_upper(domain_name) + "BackendDispatcher"
            agent_interface_name = dispatcher_name + "Handler"

            if "commands" in json_domain:
                Generator.backend_dispatcher_interface_list.append("class %s %s final : public Inspector::InspectorSupplementalBackendDispatcher {\n" % (INSPECTOR_TYPES_GENERATOR_CONFIG_MAP[output_type]["export_macro"], dispatcher_name))
                Generator.backend_dispatcher_interface_list.append("public:\n")
                Generator.backend_dispatcher_interface_list.append("    static PassRefPtr<%s> create(Inspector::InspectorBackendDispatcher*, %s*);\n" % (dispatcher_name, agent_interface_name))
                Generator.backend_dispatcher_interface_list.append("    virtual void dispatch(long callId, const String& method, PassRefPtr<Inspector::InspectorObject> message) override;\n")
                Generator.backend_dispatcher_interface_list.append("private:\n")

                Generator.backend_handler_interface_list.append("class %s %s {\n" % (INSPECTOR_TYPES_GENERATOR_CONFIG_MAP[output_type]["export_macro"], agent_interface_name))
                Generator.backend_handler_interface_list.append("public:\n")

                backend_method_count = len(Generator.backend_method_implementation_list)

                dispatcher_if_chain = []
                dispatcher_commands_list = []
                for json_command in json_domain["commands"]:
                    Generator.process_command(json_command, domain_name, agent_interface_name, dispatcher_name, dispatcher_if_chain, dispatcher_commands_list)

                Generator.backend_handler_interface_list.append("protected:\n")
                Generator.backend_handler_interface_list.append("    virtual ~%s();\n" % agent_interface_name)
                Generator.backend_handler_interface_list.append("};\n\n")

                Generator.backend_handler_implementation_list.append("%s::~%s() { }\n" % (agent_interface_name, agent_interface_name))

                Generator.backend_dispatcher_interface_list.append("private:\n")
                Generator.backend_dispatcher_interface_list.append("    %s(Inspector::InspectorBackendDispatcher*, %s*);\n" % (dispatcher_name, agent_interface_name))
                Generator.backend_dispatcher_interface_list.append("    %s* m_agent;\n" % agent_interface_name)
                Generator.backend_dispatcher_interface_list.append("};\n\n")

                Generator.backend_method_implementation_list.insert(backend_method_count, Templates.backend_dispatcher_constructor.substitute(None,
                    domainName=domain_name,
                    dispatcherName=dispatcher_name,
                    agentName=agent_interface_name))

                if len(json_domain["commands"]) <= 5:
                    Generator.backend_method_implementation_list.insert(backend_method_count + 1, Templates.backend_dispatcher_dispatch_method_simple.substitute(None,
                        domainName=domain_name,
                        dispatcherName=dispatcher_name,
                        ifChain="\n".join(dispatcher_if_chain)))
                else:
                    Generator.backend_method_implementation_list.insert(backend_method_count + 1, Templates.backend_dispatcher_dispatch_method.substitute(None,
                        domainName=domain_name,
                        dispatcherName=dispatcher_name,
                        dispatcherCommands="\n".join(dispatcher_commands_list)))

                if domain_guard:
                    for l in reversed(first_cycle_guardable_list_list):
                        domain_guard.generate_close(l)
                Generator.backend_js_domain_initializer_list.append("\n")

    @staticmethod
    def process_enum(json_enum, enum_name):
        enum_members = []
        for member in json_enum["enum"]:
            enum_members.append("%s: \"%s\"" % (fix_camel_case(member), member))

        Generator.backend_js_domain_initializer_list.append("InspectorBackend.registerEnum(\"%s\", {%s});\n" % (
            enum_name, ", ".join(enum_members)))

    @staticmethod
    def process_event(json_event, domain_name, frontend_method_declaration_lines):
        event_name = json_event["name"]

        ad_hoc_type_output = []
        frontend_method_declaration_lines.append(ad_hoc_type_output)
        ad_hoc_type_writer = Writer(ad_hoc_type_output, "        ")

        decl_parameter_list = []

        json_parameters = json_event.get("parameters")
        Generator.generate_send_method(json_parameters, event_name, domain_name, ad_hoc_type_writer,
                                       decl_parameter_list,
                                       Generator.EventMethodStructTemplate,
                                       Generator.frontend_method_list, Templates.frontend_method, {"eventName": event_name})

        backend_js_event_param_list = []
        if json_parameters:
            for parameter in json_parameters:
                parameter_name = parameter["name"]
                backend_js_event_param_list.append("\"%s\"" % parameter_name)

        frontend_method_declaration_lines.append(
            "    void %s(%s);\n" % (event_name, ", ".join(decl_parameter_list)))

        Generator.backend_js_domain_initializer_list.append("InspectorBackend.registerEvent(\"%s.%s\", [%s]);\n" % (
            domain_name, event_name, ", ".join(backend_js_event_param_list)))

    class EventMethodStructTemplate:
        @staticmethod
        def append_prolog(line_list):
            line_list.append("    RefPtr<InspectorObject> paramsObject = InspectorObject::create();\n")

        @staticmethod
        def append_epilog(line_list):
            line_list.append("    jsonMessage->setObject(ASCIILiteral(\"params\"), paramsObject);\n")

        container_name = "paramsObject"

    @staticmethod
    def process_command(json_command, domain_name, agent_interface_name, dispatcher_name, dispatcher_if_chain, dispatcher_commands_list):
        json_command_name = json_command["name"]

        ad_hoc_type_output = []
        Generator.backend_handler_interface_list.append(ad_hoc_type_output)
        ad_hoc_type_writer = Writer(ad_hoc_type_output, "    ")

        Generator.backend_dispatcher_interface_list.append("    void %s(long callId, const Inspector::InspectorObject& message);\n" % json_command_name)

        Generator.backend_handler_interface_list.append("    virtual void %s(ErrorString*" % json_command_name)

        if not dispatcher_if_chain:
            dispatcher_if_chain.append("    if (method == \"%s\")" % json_command_name)
        else:
            dispatcher_if_chain.append("    else if (method == \"%s\")" % json_command_name)
        dispatcher_if_chain.append("        %s(callId, *message.get());" % json_command_name)
        dispatcher_commands_list.append("            { \"%s\", &%s::%s }," % (json_command_name, dispatcher_name, json_command_name))

        method_in_params_handling = []
        method_dispatch_handling = []
        method_out_params_handling = []
        method_ending_handling = []
        method_request_message_param_name = ""
        agent_call_param_list = []
        js_parameters_text = ""
        if "parameters" in json_command:
            json_params = json_command["parameters"]
            method_request_message_param_name = " message"
            method_in_params_handling.append("    RefPtr<InspectorArray> protocolErrors = InspectorArray::create();\n")
            method_in_params_handling.append("    RefPtr<InspectorObject> paramsContainer = message.getObject(ASCIILiteral(\"params\"));\n")
            method_in_params_handling.append("    InspectorObject* paramsContainerPtr = paramsContainer.get();\n")
            method_in_params_handling.append("    InspectorArray* protocolErrorsPtr = protocolErrors.get();\n")
            js_param_list = []

            for json_parameter in json_params:
                json_param_name = json_parameter["name"]
                param_raw_type = resolve_param_raw_type(json_parameter, domain_name)

                getter_name = param_raw_type.get_getter_name()

                optional = json_parameter.get("optional")

                non_optional_type_model = param_raw_type.get_raw_type_model()
                if optional:
                    type_model = non_optional_type_model.get_optional()
                else:
                    type_model = non_optional_type_model

                if optional:
                    code = ("    bool %s_valueFound = false;\n"
                            "    %s in_%s = InspectorBackendDispatcher::get%s(paramsContainerPtr, ASCIILiteral(\"%s\"), &%s_valueFound, protocolErrorsPtr);\n" %
                           (json_param_name, non_optional_type_model.get_command_return_pass_model().get_return_var_type(), json_param_name, getter_name, json_param_name, json_param_name))
                    param = ", %s_valueFound ? &in_%s : nullptr" % (json_param_name, json_param_name)
                    # FIXME: pass optional refptr-values as PassRefPtr
                    formal_param_type_pattern = "const %s*"
                else:
                    code = ("    %s in_%s = InspectorBackendDispatcher::get%s(paramsContainerPtr, ASCIILiteral(\"%s\"), nullptr, protocolErrorsPtr);\n" %
                            (non_optional_type_model.get_command_return_pass_model().get_return_var_type(), json_param_name, getter_name, json_param_name))
                    param = ", in_%s" % json_param_name
                    # FIXME: pass not-optional refptr-values as NonNullPassRefPtr
                    if param_raw_type.is_heavy_value():
                        formal_param_type_pattern = "const %s&"
                    else:
                        formal_param_type_pattern = "%s"

                method_in_params_handling.append(code)
                agent_call_param_list.append(param)
                Generator.backend_handler_interface_list.append(", %s in_%s" % (formal_param_type_pattern % non_optional_type_model.get_command_return_pass_model().get_return_var_type(), json_param_name))

                js_bind_type = param_raw_type.get_js_bind_type()
                js_param_text = "{\"name\": \"%s\", \"type\": \"%s\", \"optional\": %s}" % (
                    json_param_name,
                    js_bind_type,
                    ("true" if ("optional" in json_parameter and json_parameter["optional"]) else "false"))

                js_param_list.append(js_param_text)

            method_in_params_handling.append("    if (protocolErrors->length()) {\n")
            method_in_params_handling.append("        String errorMessage = String::format(\"Some arguments of method '%%s' can't be processed\", \"%s.%s\");\n" % (domain_name, json_command_name))
            method_in_params_handling.append("        m_backendDispatcher->reportProtocolError(&callId, InspectorBackendDispatcher::InvalidParams, errorMessage, protocolErrors.release());\n")
            method_in_params_handling.append("        return;\n")
            method_in_params_handling.append("    }\n")
            method_in_params_handling.append("\n")

            js_parameters_text = ", ".join(js_param_list)

        method_dispatch_handling.append("    ErrorString error;\n")
        method_dispatch_handling.append("    RefPtr<InspectorObject> result = InspectorObject::create();\n")

        if json_command.get("async") == True:
            callback_name = Capitalizer.lower_camel_case_to_upper(json_command_name) + "Callback"

            callback_output = []
            callback_writer = Writer(callback_output, ad_hoc_type_writer.get_indent())

            decl_parameter_list = []
            Generator.generate_send_method(json_command.get("returns"), json_command_name, domain_name, ad_hoc_type_writer,
                                           decl_parameter_list,
                                           Generator.CallbackMethodStructTemplate,
                                           Generator.backend_method_implementation_list, Templates.callback_method,
                                           {"callbackName": callback_name, "handlerName": agent_interface_name})

            callback_writer.newline("class " + callback_name + " : public Inspector::InspectorBackendDispatcher::CallbackBase {\n")
            callback_writer.newline("public:\n")
            callback_writer.newline("    " + callback_name + "(PassRefPtr<Inspector::InspectorBackendDispatcher>, int id);\n")
            callback_writer.newline("    void sendSuccess(" + ", ".join(decl_parameter_list) + ");\n")
            callback_writer.newline("};\n")

            ad_hoc_type_output.append(callback_output)

            method_dispatch_handling.append("    RefPtr<%s::%s> callback = adoptRef(new %s::%s(m_backendDispatcher,callId));\n" % (agent_interface_name, callback_name, agent_interface_name, callback_name))
            method_ending_handling.append("    if (error.length()) {\n")
            method_ending_handling.append("        callback->disable();\n")
            method_ending_handling.append("        m_backendDispatcher->reportProtocolError(&callId, Inspector::InspectorBackendDispatcher::ServerError, error);\n")
            method_ending_handling.append("        return;\n")
            method_ending_handling.append("    }")

            agent_call_param_list.append(", callback")
            Generator.backend_handler_interface_list.append(", PassRefPtr<%s> callback" % callback_name)
        elif "returns" in json_command:
            has_multiple_returns = len(json_command["returns"]) > 1
            if has_multiple_returns:
                method_out_params_handling.append("    if (!error.length()) {\n")
            else:
                method_out_params_handling.append("    if (!error.length())\n")

            for json_return in json_command["returns"]:

                json_return_name = json_return["name"]

                optional = bool(json_return.get("optional"))

                return_type_binding = Generator.resolve_type_and_generate_ad_hoc(json_return, json_command_name, domain_name, ad_hoc_type_writer, agent_interface_name + "::")

                raw_type = return_type_binding.reduce_to_raw_type()
                setter_type = raw_type.get_setter_name()
                initializer = raw_type.get_c_initializer()

                type_model = return_type_binding.get_type_model()
                if optional:
                    type_model = type_model.get_optional()

                code = "    %s out_%s;\n" % (type_model.get_command_return_pass_model().get_return_var_type(), json_return_name)
                param = ", %sout_%s" % (type_model.get_command_return_pass_model().get_output_argument_prefix(), json_return_name)
                var_name = "out_%s" % json_return_name
                setter_argument = type_model.get_command_return_pass_model().get_output_to_raw_expression() % var_name
                if return_type_binding.get_setter_value_expression_pattern():
                    setter_argument = return_type_binding.get_setter_value_expression_pattern() % (INSPECTOR_TYPES_GENERATOR_CONFIG_MAP[output_type]["prefix"], setter_argument)

                cook = "        result->set%s(ASCIILiteral(\"%s\"), %s);\n" % (setter_type, json_return_name, setter_argument)

                set_condition_pattern = type_model.get_command_return_pass_model().get_set_return_condition()
                if set_condition_pattern:
                    cook = ("        if (%s)\n    " % (set_condition_pattern % var_name)) + cook
                annotated_type = type_model.get_command_return_pass_model().get_output_parameter_type()

                param_name = "out_%s" % json_return_name
                if optional:
                    param_name = "opt_" + param_name

                Generator.backend_handler_interface_list.append(", %s %s" % (annotated_type, param_name))
                method_out_params_handling.append(cook)
                method_dispatch_handling.append(code)

                agent_call_param_list.append(param)

            if has_multiple_returns:
                method_out_params_handling.append("    }\n")
            method_out_params_handling.append("\n")

        method_dispatch_handling.append("    m_agent->%s(&error%s);\n" % (json_command_name, "".join(agent_call_param_list)))
        method_dispatch_handling.append("\n")

        if not method_ending_handling:
            method_ending_handling.append("    m_backendDispatcher->sendResponse(callId, result.release(), error);")

        backend_js_reply_param_list = []
        if "returns" in json_command:
            for json_return in json_command["returns"]:
                json_return_name = json_return["name"]
                backend_js_reply_param_list.append("\"%s\"" % json_return_name)

        js_reply_list = "[%s]" % ", ".join(backend_js_reply_param_list)

        Generator.backend_method_implementation_list.append(Templates.backend_method.substitute(None,
            dispatcherName=dispatcher_name,
            methodName=json_command_name,
            requestMessageObject=method_request_message_param_name,
            methodInParametersHandling="".join(method_in_params_handling),
            methodDispatchHandling="".join(method_dispatch_handling),
            methodOutParametersHandling="".join(method_out_params_handling),
            methodEndingHandling="".join(method_ending_handling)))

        Generator.backend_js_domain_initializer_list.append("InspectorBackend.registerCommand(\"%s.%s\", [%s], %s);\n" % (domain_name, json_command_name, js_parameters_text, js_reply_list))
        Generator.backend_handler_interface_list.append(") = 0;\n")

    class CallbackMethodStructTemplate:
        @staticmethod
        def append_prolog(line_list):
            pass

        @staticmethod
        def append_epilog(line_list):
            pass

        container_name = "jsonMessage"

    # Generates common code for event sending and callback response data sending.
    @staticmethod
    def generate_send_method(parameters, event_name, domain_name, ad_hoc_type_writer, decl_parameter_list,
                             method_struct_template,
                             generator_method_list, method_template, template_params):
        method_line_list = []
        if parameters:
            method_struct_template.append_prolog(method_line_list)
            for json_parameter in parameters:
                parameter_name = json_parameter["name"]

                param_type_binding = Generator.resolve_type_and_generate_ad_hoc(json_parameter, event_name, domain_name, ad_hoc_type_writer, "")

                raw_type = param_type_binding.reduce_to_raw_type()
                raw_type_binding = RawTypeBinding(raw_type)

                optional = bool(json_parameter.get("optional"))

                setter_type = raw_type.get_setter_name()

                type_model = param_type_binding.get_type_model()
                raw_type_model = raw_type_binding.get_type_model()
                if optional:
                    type_model = type_model.get_optional()
                    raw_type_model = raw_type_model.get_optional()

                annotated_type = type_model.get_input_param_type_text()
                mode_type_binding = param_type_binding

                decl_parameter_list.append("%s %s" % (annotated_type, parameter_name))

                setter_argument = raw_type_model.get_event_setter_expression_pattern() % parameter_name
                if mode_type_binding.get_setter_value_expression_pattern():
                    setter_argument = mode_type_binding.get_setter_value_expression_pattern() % (INSPECTOR_TYPES_GENERATOR_CONFIG_MAP[output_type]["prefix"], setter_argument)

                setter_code = "    %s->set%s(ASCIILiteral(\"%s\"), %s);\n" % (method_struct_template.container_name, setter_type, parameter_name, setter_argument)
                if optional:
                    setter_code = ("    if (%s)\n    " % parameter_name) + setter_code
                method_line_list.append(setter_code)

            method_struct_template.append_epilog(method_line_list)

        generator_method_list.append(method_template.substitute(None,
            domainName=domain_name,
            parameters=", ".join(decl_parameter_list),
            code="".join(method_line_list), **template_params))

    @staticmethod
    def resolve_type_and_generate_ad_hoc(json_param, method_name, domain_name, ad_hoc_type_writer, container_relative_name_prefix_param):
        param_name = json_param["name"]
        ad_hoc_type_list = []

        class AdHocTypeContext:
            container_full_name_prefix = "<not yet defined>"
            container_relative_name_prefix = container_relative_name_prefix_param

            @staticmethod
            def get_type_name_fix():
                class NameFix:
                    class_name = Capitalizer.lower_camel_case_to_upper(param_name)

                    @staticmethod
                    def output_comment(writer):
                        writer.newline("// Named after parameter '%s' while generating command/event %s.\n" % (param_name, method_name))

                return NameFix

            @staticmethod
            def add_type(binding):
                ad_hoc_type_list.append(binding)

        type_binding = resolve_param_type(json_param, domain_name, AdHocTypeContext)

        class InterfaceForwardListener:
            @staticmethod
            def add_type_data(type_data):
                pass

        class InterfaceResolveContext:
            forward_listener = InterfaceForwardListener

        for type in ad_hoc_type_list:
            type.resolve_inner(InterfaceResolveContext)

        class InterfaceGenerateContext:
            validator_writer = "not supported in InterfaceGenerateContext"
            cpp_writer = validator_writer

        for type in ad_hoc_type_list:
            generator = type.get_code_generator()
            if generator:
                generator.generate_type_builder(ad_hoc_type_writer, InterfaceGenerateContext)

        return type_binding

    @staticmethod
    def process_types(type_map):
        output = Generator.type_builder_fragments

        class GenerateContext:
            validator_writer = Writer(Generator.validator_impl_list, "")
            cpp_writer = Writer(Generator.type_builder_impl_list, "")

        def generate_all_domains_code(out, type_data_callback):
            writer = Writer(out, "")
            for domain_data in type_map.domains_to_generate():
                domain_fixes = DomainNameFixes.get_fixed_data(domain_data.name())
                domain_guard = domain_fixes.get_guard()

                namespace_declared = []

                def namespace_lazy_generator():
                    if not namespace_declared:
                        if domain_guard:
                            domain_guard.generate_open(out)
                        writer.newline("namespace ")
                        writer.append(domain_data.name())
                        writer.append(" {\n")
                        # What is a better way to change value from outer scope?
                        namespace_declared.append(True)
                    return writer

                for type_data in domain_data.types():
                    type_data_callback(type_data, namespace_lazy_generator)

                if namespace_declared:
                    writer.append("} // ")
                    writer.append(domain_data.name())
                    writer.append("\n\n")

                    if domain_guard:
                        domain_guard.generate_close(out)

        def create_type_builder_caller(generate_pass_id):
            def call_type_builder(type_data, writer_getter):
                code_generator = type_data.get_binding().get_code_generator()
                if code_generator and generate_pass_id == code_generator.get_generate_pass_id():
                    writer = writer_getter()

                    code_generator.generate_type_builder(writer, GenerateContext)
            return call_type_builder

        generate_all_domains_code(output, create_type_builder_caller(TypeBuilderPass.MAIN))

        Generator.type_builder_forwards.append("// Forward declarations.\n")

        def generate_forward_callback(type_data, writer_getter):
            if type_data in global_forward_listener.type_data_set:
                binding = type_data.get_binding()
                binding.get_code_generator().generate_forward_declaration(writer_getter())
        generate_all_domains_code(Generator.type_builder_forwards, generate_forward_callback)

        Generator.type_builder_forwards.append("// End of forward declarations.\n\n")

        Generator.type_builder_forwards.append("// Typedefs.\n")

        generate_all_domains_code(Generator.type_builder_forwards, create_type_builder_caller(TypeBuilderPass.TYPEDEF))

        Generator.type_builder_forwards.append("// End of typedefs.\n\n")


def flatten_list(input):
    res = []

    def fill_recursive(l):
        for item in l:
            if isinstance(item, list):
                fill_recursive(item)
            else:
                res.append(item)
    fill_recursive(input)
    return res


# A writer that only updates file if it actually changed to better support incremental build.
class SmartOutput:
    def __init__(self, file_name):
        self.file_name_ = file_name
        self.output_ = ""

    def write(self, text):
        self.output_ += text

    def close(self):
        text_changed = True
        self.output_ = self.output_.rstrip() + "\n"

        try:
            read_file = open(self.file_name_, "r")
            old_text = read_file.read()
            read_file.close()
            text_changed = old_text != self.output_
        except:
            # Ignore, just overwrite by default
            pass

        if text_changed or write_always:
            out_file = open(self.file_name_, "w")
            out_file.write(self.output_)
            out_file.close()


Generator.go()

output_file_name_prefix = INSPECTOR_TYPES_GENERATOR_CONFIG_MAP[output_type]["prefix"]

backend_h_file = SmartOutput(output_header_dirname + ("/Inspector%sBackendDispatchers.h" % output_file_name_prefix))
backend_cpp_file = SmartOutput(output_cpp_dirname + ("/Inspector%sBackendDispatchers.cpp" % output_file_name_prefix))

frontend_h_file = SmartOutput(output_header_dirname + ("/Inspector%sFrontendDispatchers.h" % output_file_name_prefix))
frontend_cpp_file = SmartOutput(output_cpp_dirname + ("/Inspector%sFrontendDispatchers.cpp" % output_file_name_prefix))

typebuilder_h_file = SmartOutput(output_header_dirname + ("/Inspector%sTypeBuilders.h" % output_file_name_prefix))
typebuilder_cpp_file = SmartOutput(output_cpp_dirname + ("/Inspector%sTypeBuilders.cpp" % output_file_name_prefix))

backend_js_file = SmartOutput(output_js_dirname + ("/Inspector%sBackendCommands.js" % output_file_name_prefix))


backend_h_file.write(Templates.backend_h.substitute(None,
    outputFileNamePrefix=output_file_name_prefix,
    handlerInterfaces="".join(flatten_list(Generator.backend_handler_interface_list)),
    dispatcherInterfaces="".join(flatten_list(Generator.backend_dispatcher_interface_list))))

backend_cpp_file.write(Templates.backend_cpp.substitute(None,
    outputFileNamePrefix=output_file_name_prefix,
    handlerImplementations="".join(flatten_list(Generator.backend_handler_implementation_list)),
    methods="\n".join(Generator.backend_method_implementation_list)))

frontend_h_file.write(Templates.frontend_h.substitute(None,
    outputFileNamePrefix=output_file_name_prefix,
    domainClassList="".join(Generator.frontend_domain_class_lines)))

frontend_cpp_file.write(Templates.frontend_cpp.substitute(None,
    outputFileNamePrefix=output_file_name_prefix,
    methods="\n".join(Generator.frontend_method_list)))

typebuilder_h_file.write(Templates.typebuilder_h.substitute(None,
    outputFileNamePrefix=output_file_name_prefix,
    typeBuilderDependencies=INSPECTOR_TYPES_GENERATOR_CONFIG_MAP[output_type]["typebuilder_dependency"],
    exportMacro=INSPECTOR_TYPES_GENERATOR_CONFIG_MAP[output_type]["export_macro"],
    typeBuilders="".join(flatten_list(Generator.type_builder_fragments)),
    forwards="".join(Generator.type_builder_forwards),
    validatorIfdefName=VALIDATOR_IFDEF_NAME))

typebuilder_cpp_file.write(Templates.typebuilder_cpp.substitute(None,
    outputFileNamePrefix=output_file_name_prefix,
    enumConstantValues=EnumConstants.get_enum_constant_code(),
    implCode="".join(flatten_list(Generator.type_builder_impl_list)),
    validatorCode="".join(flatten_list(Generator.validator_impl_list)),
    validatorIfdefName=VALIDATOR_IFDEF_NAME))

backend_js_file.write(Templates.backend_js.substitute(None,
    domainInitializers="".join(Generator.backend_js_domain_initializer_list)))

backend_h_file.close()
backend_cpp_file.close()

frontend_h_file.close()
frontend_cpp_file.close()

typebuilder_h_file.close()
typebuilder_cpp_file.close()

backend_js_file.close()
