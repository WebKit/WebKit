#!/usr/bin/env python
# Copyright (c) 2011 Google Inc. All rights reserved.
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
from string import join
try:
    import json
except ImportError:
    import simplejson as json


DOMAIN_DEFINE_NAME_MAP = {
    "Database": "SQL_DATABASE",
    "Debugger": "JAVASCRIPT_DEBUGGER",
    "DOMDebugger": "JAVASCRIPT_DEBUGGER",
    "FileSystem": "FILE_SYSTEM",
    "Profiler": "JAVASCRIPT_DEBUGGER",
    "Worker": "WORKERS",
}


# Manually-filled map of type name replacements.
TYPE_NAME_FIX_MAP = {
    "RGBA": "Rgba",  # RGBA is reported to be conflicting with a define name in Windows CE.
}


cmdline_parser = optparse.OptionParser()
# FIXME: get rid of this option once the system is stable.
cmdline_parser.add_option("--defines")
cmdline_parser.add_option("--output_h_dir")
cmdline_parser.add_option("--output_cpp_dir")

try:
    arg_options, arg_values = cmdline_parser.parse_args()
    if (len(arg_values) != 1):
        raise Exception("Exactly one plain argument expected (found %s)" % len(arg_values))
    input_json_filename = arg_values[0]
    output_header_dirname = arg_options.output_h_dir
    output_cpp_dirname = arg_options.output_cpp_dir
    if not output_header_dirname:
        raise Exception("Output .h directory must be specified")
    if not output_cpp_dirname:
        raise Exception("Output .cpp directory must be specified")
except Exception, e:
    sys.stderr.write("Failed to parse command-line arguments: %s\n\n" % e)
    sys.stderr.write("Usage: <script> Inspector.json --output_h_dir <output_header_dir> --output_cpp_dir <output_cpp_dir> [--defines <defines string>]\n")
    exit(1)


def dash_to_camelcase(word):
    return ''.join(x.capitalize() or '-' for x in word.split('-'))

def parse_defines(str):
    if not str:
        return {}

    items = str.split()
    result = {}
    for item in items:
        if item[0] == '"' and item[-1] == '"' and len(item) >= 2:
            item = item[1:-1]
        eq_pos = item.find("=")
        if eq_pos == -1:
            key = item
            value = True
        else:
            key = item[:eq_pos]
            value_str = item[eq_pos + 1:]
            if value_str == "0":
                value = False
            elif value_str == "1":
                value = True
            else:
                # Should we support other values?
                raise Exception("Unrecognized define value: '%s' (key: '%s')" % (value_str, key))
        result[key] = value
    return result

defines_map = parse_defines(arg_options.defines)


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


class DomainNameFixes:
    @classmethod
    def get_fixed_data(cls, domain_name):
        if domain_name in cls.agent_type_map:
            agent_name_res = cls.agent_type_map[domain_name]
        else:
            agent_name_res = "Inspector%sAgent" % domain_name

        field_name_res = Capitalizer.upper_camel_case_to_lower(domain_name) + "Agent"

        class Res(object):
            agent_type_name = agent_name_res
            skip_js_bind = domain_name in cls.skip_js_bind_domains
            agent_field_name = field_name_res

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

    skip_js_bind_domains = set(["Runtime", "DOMDebugger"])
    agent_type_map = {"Network": "InspectorResourceAgent", "Inspector": "InspectorAgent", }


class CParamType(object):
    def __init__(self, type, setter_format="%s"):
        self.type = type
        self.setter_format = setter_format

    def get_text(self):
        return self.type

    def get_setter_format(self):
        return self.setter_format


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

    class BaseType(object):
        @classmethod
        def get_c_param_type(cls, param_type, optional):
            return cls.default_c_param_type

        @staticmethod
        def is_event_param_check_optional():
            return False

    class String(BaseType):
        @classmethod
        def get_c_param_type(cls, param_type, optional):
            if param_type == ParamType.EVENT or param_type == ParamType.TYPE_BUILDER_OUTPUT:
                return cls._ref_c_type
            else:
                return cls._plain_c_type

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

        _plain_c_type = CParamType("String")
        _ref_c_type = CParamType("const String&")

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

        default_c_param_type = CParamType("int")

    class Number(BaseType):
        @staticmethod
        def get_getter_name():
            return "Object"

        @staticmethod
        def get_setter_name():
            return "Number"

        @staticmethod
        def get_c_initializer():
            raise Exception("Unsupported")

        @staticmethod
        def get_js_bind_type():
            raise Exception("Unsupported")

        default_c_param_type = CParamType("double")

    class Bool(BaseType):
        @classmethod
        def get_c_param_type(cls, param_type, optional):
            if (param_type == ParamType.EVENT):
                if optional:
                    return cls._ref_c_type
                else:
                    return cls._plain_c_type
            else:
                return cls._plain_c_type

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
        def is_event_param_check_optional():
            return True

        _plain_c_type = CParamType("bool")
        _ref_c_type = CParamType("const bool* const", "*%s")

    class Object(BaseType):
        @classmethod
        def get_c_param_type(cls, param_type, optional):
            if param_type == ParamType.EVENT or param_type == ParamType.TYPE_BUILDER_OUTPUT:
                return cls._ref_c_type
            else:
                return cls._plain_c_type

        @staticmethod
        def get_getter_name():
            return "Object"

        get_setter_name = get_getter_name

        @staticmethod
        def get_c_initializer():
            return "InspectorObject::create()"

        @staticmethod
        def get_js_bind_type():
            return "object"

        @staticmethod
        def is_event_param_check_optional():
            return True

        _plain_c_type = CParamType("RefPtr<InspectorObject>")
        _ref_c_type = CParamType("PassRefPtr<InspectorObject>")

    class Any(BaseType):
        @classmethod
        def get_c_param_type(cls, param_type, optional):
            if param_type == ParamType.EVENT or param_type == ParamType.TYPE_BUILDER_OUTPUT:
                return cls._ref_c_type
            else:
                return cls._plain_c_type

        @staticmethod
        def get_getter_name():
            return "Value"

        get_setter_name = get_getter_name

        @staticmethod
        def get_c_initializer():
            return "InspectorValue::create()"

        @staticmethod
        def get_js_bind_type():
            raise Exception("Unsupported")

        @staticmethod
        def is_event_param_check_optional():
            return True

        _plain_c_type = CParamType("RefPtr<InspectorValue>")
        _ref_c_type = CParamType("PassRefPtr<InspectorValue>")

    class Array(BaseType):
        @classmethod
        def get_c_param_type(cls, param_type, optional):
            if param_type == ParamType.OUTPUT:
                return cls._plain_c_type
            elif param_type == ParamType.INPUT:
                return cls._plain_c_type
            else:
                return cls._ref_c_type

        @staticmethod
        def get_getter_name():
            return "Array"

        get_setter_name = get_getter_name

        @staticmethod
        def get_c_initializer():
            return "InspectorArray::create()"

        @staticmethod
        def get_js_bind_type():
            return "object"

        @staticmethod
        def is_event_param_check_optional():
            return True

        _plain_c_type = CParamType("RefPtr<InspectorArray>")
        _ref_c_type = CParamType("PassRefPtr<InspectorArray>")


class ParamType(object):
    INPUT = "input"
    OUTPUT = "output"
    EVENT = "event"
    TYPE_BUILDER_OUTPUT = "typeBuilderOutput"

# Collection of InspectorObject class methods that are likely to be overloaded in generated class.
# We must explicitly import all overloaded methods or they won't be available to user.
INSPECTOR_OBJECT_SETTER_NAMES = frozenset(["setValue", "setBoolean", "setNumber", "setString", "setValue", "setObject", "setArray"])


def fix_type_name(json_name):
    if json_name in TYPE_NAME_FIX_MAP:
        fixed = TYPE_NAME_FIX_MAP[json_name]

        class Result(object):
            class_name = fixed

            @staticmethod
            def output_comment(output):
                output.append("// Type originally was named '%s'.\n" % json_name)
    else:

        class Result(object):
            class_name = json_name

            @staticmethod
            def output_comment(output):
                pass

    return Result



class TypeBindings:
    @staticmethod
    def create_for_named_type_declaration(json_type, context_domain_name):
        fixed_type_name = fix_type_name(json_type["id"])

        def write_doc(output):
            if "description" in json_type:
                output.append("/* ")
                output.append(json_type["description"])
                output.append(" */\n")

        if json_type["type"] == "string":
            if "enum" in json_type:

                class EnumBinding:
                    @staticmethod
                    def generate_type_builder(output, forward_listener):
                        enum = json_type["enum"]
                        write_doc(output)
                        enum_name = fixed_type_name.class_name
                        fixed_type_name.output_comment(output)
                        output.append("namespace ")
                        output.append(enum_name)
                        output.append(" {\n")
                        for enum_item in enum:
                            item_c_name = enum_item.replace('-', '_')
                            output.append("const char* const ")
                            output.append(Capitalizer.upper_camel_case_to_lower(item_c_name))
                            output.append(" = \"")
                            output.append(enum_item)
                            output.append("\";\n")
                        output.append("} // namespace ")
                        output.append(enum_name)
                        output.append("\n\n")

                return EnumBinding
            else:

                class PlainString:
                    @staticmethod
                    def generate_type_builder(output, forward_listener):
                        write_doc(output)
                        fixed_type_name.output_comment(output)
                        output.append("typedef String ")
                        output.append(fixed_type_name.class_name)
                        output.append(";\n\n")
                return PlainString

        elif json_type["type"] == "object":
            if "properties" in json_type:

                class ClassBinding:
                    @staticmethod
                    def generate_type_builder(output, forward_listener):
                        write_doc(output)
                        class_name = fixed_type_name.class_name
                        fixed_type_name.output_comment(output)
                        output.append("class ")
                        output.append(class_name)
                        output.append(" : public InspectorObject {\n")
                        output.append("public:\n")

                        properties = json_type["properties"]
                        main_properties = []
                        optional_properties = []
                        for p in properties:
                            if "optional" in p and p["optional"]:
                                optional_properties.append(p)
                            else:
                                main_properties.append(p)

                        output.append(
"""    enum {
        NO_FIELDS_SET = 0,
""")

                        state_enum_items = []
                        if len(main_properties) > 0:
                            pos = 0
                            for p in main_properties:
                                item_name = Capitalizer.camel_case_to_capitalized_with_underscores(p["name"]) + "_SET"
                                state_enum_items.append(item_name)
                                output.append("        %s = 1 << %s,\n" % (item_name, pos))
                                pos += 1
                            all_fields_set_value = "(" + (" | ".join(state_enum_items)) + ")"
                        else:
                            all_fields_set_value = "0"

                        output.append(
"""        ALL_FIELDS_SET = %s
    };

    template<int STATE>
    class Builder {
    private:
        RefPtr<InspectorObject> m_result;

        template<int STEP> Builder<STATE | STEP>& castState()
        {
            return *reinterpret_cast<Builder<STATE | STEP>*>(this);
        }

        Builder(PassRefPtr<%s> ptr)
        {
            COMPILE_ASSERT(STATE == NO_FIELDS_SET, builder_created_in_non_init_state);
            m_result = ptr;
        }
        friend class %s;
    public:
""" % (all_fields_set_value, class_name, class_name))

                        pos = 0
                        for prop in main_properties:
                            prop_name = prop["name"]
                            param_raw_type = resolve_param_raw_type(prop, context_domain_name)
                            output.append("""
        Builder<STATE | %s>& set%s(%s value)
        {
            COMPILE_ASSERT(!(STATE & %s), property_%s_already_set);
            m_result->set%s("%s", value);
            return castState<%s>();
        }
"""
                            % (state_enum_items[pos],
                               Capitalizer.lower_camel_case_to_upper(prop_name),
                               param_raw_type.get_c_param_type(ParamType.TYPE_BUILDER_OUTPUT, False).get_text(),
                               state_enum_items[pos], prop_name,
                               param_raw_type.get_setter_name(), prop_name, state_enum_items[pos]))

                            pos += 1

                        output.append("""
        operator RefPtr<%s>& ()
        {
            COMPILE_ASSERT(STATE == ALL_FIELDS_SET, result_is_not_ready);
            return *reinterpret_cast<RefPtr<%s>*>(&m_result);
        }

        operator PassRefPtr<%s> ()
        {
            return RefPtr<%s>(*this);
        }
    };

"""
                        % (class_name, class_name, class_name, class_name))

                        output.append("    /*\n")
                        output.append("     * Synthetic constructor:\n")
                        output.append("     * RefPtr<%s> result = %s::create()" % (class_name, class_name))
                        for prop in main_properties:
                            output.append("\n     *     .set%s(...)" % Capitalizer.lower_camel_case_to_upper(prop["name"]))
                        output.append(";\n     */\n")

                        output.append(
"""    static Builder<NO_FIELDS_SET> create()
    {
        return Builder<NO_FIELDS_SET>(adoptRef(new %s()));
    }
""" % class_name)

                        for prop in optional_properties:
                            param_raw_type = resolve_param_raw_type(prop, context_domain_name)
                            setter_name = "set%s" % Capitalizer.lower_camel_case_to_upper(prop["name"])
                            output.append("\n    void %s" % setter_name)
                            output.append("(%s value)\n" % param_raw_type.get_c_param_type(ParamType.TYPE_BUILDER_OUTPUT, False).get_text())
                            output.append("    {\n")
                            output.append("        this->set%s(\"%s\", value);\n" % (param_raw_type.get_setter_name(), prop["name"]))
                            output.append("    }\n")

                            if setter_name in INSPECTOR_OBJECT_SETTER_NAMES:
                                output.append("    using InspectorObject::%s;\n\n" % setter_name)

                        output.append("};\n\n")
                return ClassBinding
            else:

                class PlainObjectBinding:
                    @staticmethod
                    def generate_type_builder(output, forward_listener):
                        # No-op
                        pass
                return PlainObjectBinding
        else:
            raw_type = RawTypes.get(json_type["type"])

            class RawTypesBinding:
                @staticmethod
                def generate_type_builder(output, forward_listener):
                    # No-op
                    pass
            return RawTypesBinding


class TypeData(object):
    def __init__(self, json_type, json_domain, domain_data):
        self.json_type_ = json_type
        self.json_domain_ = json_domain
        self.domain_data_ = domain_data

        if "type" not in json_type:
            raise Exception("Unknown type")

        json_type_name = json_type["type"]
        raw_type = RawTypes.get(json_type_name)
        self.raw_type_ = raw_type
        self.binding_ = TypeBindings.create_for_named_type_declaration(json_type, json_domain["domain"])

    def get_raw_type(self):
        return self.raw_type_

    def get_binding(self):
        return self.binding_


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
    def __init__(self, api):
        self.map_ = {}
        self.domains_ = []
        for json_domain in api["domains"]:
            domain_name = json_domain["domain"]

            domain_map = {}
            self.map_[domain_name] = domain_map

            domain_data = DomainData(json_domain)
            self.domains_.append(domain_data)

            if "types" in json_domain:
                for json_type in json_domain["types"]:
                    type_name = json_type["id"]
                    type_data = TypeData(json_type, json_domain, domain_data)
                    domain_map[type_name] = type_data
                    domain_data.add_type(type_data)

    def domains(self):
        return self.domains_

    def get(self, domain_name, type_name):
        return self.map_[domain_name][type_name]


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
"""// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
""")



    frontend_domain_class = string.Template(
"""    class $domainClassName {
    public:
        $domainClassName(InspectorFrontendChannel* inspectorFrontendChannel) : m_inspectorFrontendChannel(inspectorFrontendChannel) { }
${frontendDomainMethodDeclarations}        void setInspectorFrontendChannel(InspectorFrontendChannel* inspectorFrontendChannel) { m_inspectorFrontendChannel = inspectorFrontendChannel; }
        InspectorFrontendChannel* getInspectorFrontendChannel() { return m_inspectorFrontendChannel; }
    private:
        InspectorFrontendChannel* m_inspectorFrontendChannel;
    };

    $domainClassName* $domainFieldName() { return &m_$domainFieldName; }

""")

    backend_method = string.Template(
"""void InspectorBackendDispatcher::${domainName}_$methodName(long callId, InspectorObject*$requestMessageObject)
{
    RefPtr<InspectorArray> protocolErrors = InspectorArray::create();

    if (!$agentField)
        protocolErrors->pushString("${domainName} handler is not available.");
$methodOutCode
    ErrorString error;
$methodInCode
    if (!protocolErrors->length())
        $agentField->$methodName(&error$agentCallParams);

    RefPtr<InspectorObject> result = InspectorObject::create();
${responseCook}    sendResponse(callId, result, String::format("Some arguments of method '%s' can't be processed", "$domainName.$methodName"), protocolErrors, error);
}
""")

    frontend_method = string.Template("""void InspectorFrontend::$domainName::$eventName($parameters)
{
    RefPtr<InspectorObject> ${eventName}Message = InspectorObject::create();
    ${eventName}Message->setString("method", "$domainName.$eventName");
$code    if (m_inspectorFrontendChannel)
        m_inspectorFrontendChannel->sendMessageToFrontend(${eventName}Message->toJSONString());
}
""")

    frontend_h = string.Template(file_header_ +
"""#ifndef InspectorFrontend_h
#define InspectorFrontend_h

#include "InspectorValues.h"
#include <PlatformString.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

class InspectorFrontendChannel;

// Both InspectorObject and InspectorArray may or may not be declared at this point as defined by ENABLED_INSPECTOR.
// Double-check we have them at least as forward declaration.
class InspectorArray;
class InspectorObject;

typedef String ErrorString;

#if ENABLE(INSPECTOR)

namespace TypeBuilder {
${typeBuilders}
} // namespace TypeBuilder

#endif // ENABLE(INSPECTOR)

class InspectorFrontend {
public:
    InspectorFrontend(InspectorFrontendChannel*);


$domainClassList
private:
    InspectorFrontendChannel* m_inspectorFrontendChannel;
${fieldDeclarations}};

} // namespace WebCore
#endif // !defined(InspectorFrontend_h)
""")

    backend_h = string.Template(file_header_ +
"""#ifndef InspectorBackendDispatcher_h
#define InspectorBackendDispatcher_h

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class InspectorAgent;
class InspectorObject;
class InspectorArray;
class InspectorFrontendChannel;

$forwardDeclarations

typedef String ErrorString;

class InspectorBackendDispatcher: public RefCounted<InspectorBackendDispatcher> {
public:
    InspectorBackendDispatcher(InspectorFrontendChannel* inspectorFrontendChannel)
        : m_inspectorFrontendChannel(inspectorFrontendChannel)
$constructorInit
    { }

$setters

    void clearFrontend() { m_inspectorFrontendChannel = 0; }

    enum CommonErrorCode {
        ParseError = 0,
        InvalidRequest,
        MethodNotFound,
        InvalidParams,
        InternalError,
        ServerError,
        LastEntry,
    };

    void reportProtocolError(const long* const callId, CommonErrorCode, const String& errorMessage) const;
    void reportProtocolError(const long* const callId, CommonErrorCode, const String& errorMessage, PassRefPtr<InspectorArray> data) const;
    void dispatch(const String& message);
    static bool getCommandName(const String& message, String* result);

    enum MethodNames {
$methodNamesEnumContent
};

    static const char* commandNames[];

private:
    static int getInt(InspectorObject* object, const String& name, bool* valueFound, InspectorArray* protocolErrors);
    static String getString(InspectorObject* object, const String& name, bool* valueFound, InspectorArray* protocolErrors);
    static bool getBoolean(InspectorObject* object, const String& name, bool* valueFound, InspectorArray* protocolErrors);
    static PassRefPtr<InspectorObject> getObject(InspectorObject* object, const String& name, bool* valueFound, InspectorArray* protocolErrors);
    static PassRefPtr<InspectorArray> getArray(InspectorObject* object, const String& name, bool* valueFound, InspectorArray* protocolErrors);
    void sendResponse(long callId, PassRefPtr<InspectorObject> result, const String& errorMessage, PassRefPtr<InspectorArray> protocolErrors, ErrorString invocationError);

$methodDeclarations

    InspectorFrontendChannel* m_inspectorFrontendChannel;
$fieldDeclarations
};

} // namespace WebCore
#endif // !defined(InspectorBackendDispatcher_h)


""")

    backend_cpp = string.Template(file_header_ +
"""

#include "config.h"
#include "InspectorBackendDispatcher.h"
#include <wtf/text/WTFString.h>
#include <wtf/text/CString.h>

#if ENABLE(INSPECTOR)

#include "InspectorAgent.h"
#include "InspectorValues.h"
#include "InspectorFrontendChannel.h"
#include <wtf/text/WTFString.h>
$includes

namespace WebCore {

const char* InspectorBackendDispatcher::commandNames[] = {
$methodNameDeclarations
};


$methods
void InspectorBackendDispatcher::dispatch(const String& message)
{
    RefPtr<InspectorBackendDispatcher> protect = this;
    typedef void (InspectorBackendDispatcher::*CallHandler)(long callId, InspectorObject* messageObject);
    typedef HashMap<String, CallHandler> DispatchMap;
    DEFINE_STATIC_LOCAL(DispatchMap, dispatchMap, );
    long callId = 0;

    if (dispatchMap.isEmpty()) {
        static CallHandler handlers[] = {
$messageHandlers
        };
        size_t length = sizeof(commandNames) / sizeof(commandNames[0]);
        for (size_t i = 0; i < length; ++i)
            dispatchMap.add(commandNames[i], handlers[i]);
    }

    RefPtr<InspectorValue> parsedMessage = InspectorValue::parseJSON(message);
    if (!parsedMessage) {
        reportProtocolError(0, ParseError, "Message must be in JSON format");
        return;
    }

    RefPtr<InspectorObject> messageObject = parsedMessage->asObject();
    if (!messageObject) {
        reportProtocolError(0, InvalidRequest, "Message must be a JSONified object");
        return;
    }

    RefPtr<InspectorValue> callIdValue = messageObject->get("id");
    if (!callIdValue) {
        reportProtocolError(0, InvalidRequest, "'id' property was not found");
        return;
    }

    if (!callIdValue->asNumber(&callId)) {
        reportProtocolError(0, InvalidRequest, "The type of 'id' property must be number");
        return;
    }

    RefPtr<InspectorValue> methodValue = messageObject->get("method");
    if (!methodValue) {
        reportProtocolError(&callId, InvalidRequest, "'method' property wasn't found");
        return;
    }

    String method;
    if (!methodValue->asString(&method)) {
        reportProtocolError(&callId, InvalidRequest, "The type of 'method' property must be string");
        return;
    }

    HashMap<String, CallHandler>::iterator it = dispatchMap.find(method);
    if (it == dispatchMap.end()) {
        reportProtocolError(&callId, MethodNotFound, "'" + method + "' wasn't found");
        return;
    }

    ((*this).*it->second)(callId, messageObject.get());
}

void InspectorBackendDispatcher::sendResponse(long callId, PassRefPtr<InspectorObject> result, const String& errorMessage, PassRefPtr<InspectorArray> protocolErrors, ErrorString invocationError)
{
    if (protocolErrors->length()) {
        reportProtocolError(&callId, InvalidParams, errorMessage, protocolErrors);
        return;
    }
    if (invocationError.length()) {
        reportProtocolError(&callId, ServerError, invocationError);
        return;
    }

    RefPtr<InspectorObject> responseMessage = InspectorObject::create();
    responseMessage->setObject("result", result);
    responseMessage->setNumber("id", callId);
    if (m_inspectorFrontendChannel)
        m_inspectorFrontendChannel->sendMessageToFrontend(responseMessage->toJSONString());
}

void InspectorBackendDispatcher::reportProtocolError(const long* const callId, CommonErrorCode code, const String& errorMessage) const
{
    reportProtocolError(callId, code, errorMessage, 0);
}

void InspectorBackendDispatcher::reportProtocolError(const long* const callId, CommonErrorCode code, const String& errorMessage, PassRefPtr<InspectorArray> data) const
{
    DEFINE_STATIC_LOCAL(Vector<int>,s_commonErrors,);
    if (!s_commonErrors.size()) {
        s_commonErrors.insert(ParseError, -32700);
        s_commonErrors.insert(InvalidRequest, -32600);
        s_commonErrors.insert(MethodNotFound, -32601);
        s_commonErrors.insert(InvalidParams, -32602);
        s_commonErrors.insert(InternalError, -32603);
        s_commonErrors.insert(ServerError, -32000);
    }
    ASSERT(code >=0);
    ASSERT((unsigned)code < s_commonErrors.size());
    ASSERT(s_commonErrors[code]);
    RefPtr<InspectorObject> error = InspectorObject::create();
    error->setNumber("code", s_commonErrors[code]);
    error->setString("message", errorMessage);
    ASSERT(error);
    if (data)
        error->setArray("data", data);
    RefPtr<InspectorObject> message = InspectorObject::create();
    message->setObject("error", error);
    if (callId)
        message->setNumber("id", *callId);
    else
        message->setValue("id", InspectorValue::null());
    if (m_inspectorFrontendChannel)
        m_inspectorFrontendChannel->sendMessageToFrontend(message->toJSONString());
}

int InspectorBackendDispatcher::getInt(InspectorObject* object, const String& name, bool* valueFound, InspectorArray* protocolErrors)
{
    ASSERT(protocolErrors);

    if (valueFound)
        *valueFound = false;

    int value = 0;

    if (!object) {
        if (!valueFound) {
            // Required parameter in missing params container.
            protocolErrors->pushString(String::format("'params' object must contain required parameter '%s' with type 'Number'.", name.utf8().data()));
        }
        return value;
    }

    InspectorObject::const_iterator end = object->end();
    InspectorObject::const_iterator valueIterator = object->find(name);

    if (valueIterator == end) {
        if (!valueFound)
            protocolErrors->pushString(String::format("Parameter '%s' with type 'Number' was not found.", name.utf8().data()));
        return value;
    }

    if (!valueIterator->second->asNumber(&value))
        protocolErrors->pushString(String::format("Parameter '%s' has wrong type. It must be 'Number'.", name.utf8().data()));
    else
        if (valueFound)
            *valueFound = true;
    return value;
}

String InspectorBackendDispatcher::getString(InspectorObject* object, const String& name, bool* valueFound, InspectorArray* protocolErrors)
{
    ASSERT(protocolErrors);

    if (valueFound)
        *valueFound = false;

    String value = "";

    if (!object) {
        if (!valueFound) {
            // Required parameter in missing params container.
            protocolErrors->pushString(String::format("'params' object must contain required parameter '%s' with type 'String'.", name.utf8().data()));
        }
        return value;
    }

    InspectorObject::const_iterator end = object->end();
    InspectorObject::const_iterator valueIterator = object->find(name);

    if (valueIterator == end) {
        if (!valueFound)
            protocolErrors->pushString(String::format("Parameter '%s' with type 'String' was not found.", name.utf8().data()));
        return value;
    }

    if (!valueIterator->second->asString(&value))
        protocolErrors->pushString(String::format("Parameter '%s' has wrong type. It must be 'String'.", name.utf8().data()));
    else
        if (valueFound)
            *valueFound = true;
    return value;
}

bool InspectorBackendDispatcher::getBoolean(InspectorObject* object, const String& name, bool* valueFound, InspectorArray* protocolErrors)
{
    ASSERT(protocolErrors);

    if (valueFound)
        *valueFound = false;

    bool value = false;

    if (!object) {
        if (!valueFound) {
            // Required parameter in missing params container.
            protocolErrors->pushString(String::format("'params' object must contain required parameter '%s' with type 'Boolean'.", name.utf8().data()));
        }
        return value;
    }

    InspectorObject::const_iterator end = object->end();
    InspectorObject::const_iterator valueIterator = object->find(name);

    if (valueIterator == end) {
        if (!valueFound)
            protocolErrors->pushString(String::format("Parameter '%s' with type 'Boolean' was not found.", name.utf8().data()));
        return value;
    }

    if (!valueIterator->second->asBoolean(&value))
        protocolErrors->pushString(String::format("Parameter '%s' has wrong type. It must be 'Boolean'.", name.utf8().data()));
    else
        if (valueFound)
            *valueFound = true;
    return value;
}

PassRefPtr<InspectorObject> InspectorBackendDispatcher::getObject(InspectorObject* object, const String& name, bool* valueFound, InspectorArray* protocolErrors)
{
    ASSERT(protocolErrors);

    if (valueFound)
        *valueFound = false;

    RefPtr<InspectorObject> value = InspectorObject::create();

    if (!object) {
        if (!valueFound) {
            // Required parameter in missing params container.
            protocolErrors->pushString(String::format("'params' object must contain required parameter '%s' with type 'Object'.", name.utf8().data()));
        }
        return value;
    }

    InspectorObject::const_iterator end = object->end();
    InspectorObject::const_iterator valueIterator = object->find(name);

    if (valueIterator == end) {
        if (!valueFound)
            protocolErrors->pushString(String::format("Parameter '%s' with type 'Object' was not found.", name.utf8().data()));
        return value;
    }

    if (!valueIterator->second->asObject(&value))
        protocolErrors->pushString(String::format("Parameter '%s' has wrong type. It must be 'Object'.", name.utf8().data()));
    else
        if (valueFound)
            *valueFound = true;
    return value;
}

PassRefPtr<InspectorArray> InspectorBackendDispatcher::getArray(InspectorObject* object, const String& name, bool* valueFound, InspectorArray* protocolErrors)
{
    ASSERT(protocolErrors);

    if (valueFound)
        *valueFound = false;

    RefPtr<InspectorArray> value = InspectorArray::create();

    if (!object) {
        if (!valueFound) {
            // Required parameter in missing params container.
            protocolErrors->pushString(String::format("'params' object must contain required parameter '%s' with type 'Array'.", name.utf8().data()));
        }
        return value;
    }

    InspectorObject::const_iterator end = object->end();
    InspectorObject::const_iterator valueIterator = object->find(name);

    if (valueIterator == end) {
        if (!valueFound)
            protocolErrors->pushString(String::format("Parameter '%s' with type 'Array' was not found.", name.utf8().data()));
        return value;
    }

    if (!valueIterator->second->asArray(&value))
        protocolErrors->pushString(String::format("Parameter '%s' has wrong type. It must be 'Array'.", name.utf8().data()));
    else
        if (valueFound)
            *valueFound = true;
    return value;
}
bool InspectorBackendDispatcher::getCommandName(const String& message, String* result)
{
    RefPtr<InspectorValue> value = InspectorValue::parseJSON(message);
    if (!value)
        return false;

    RefPtr<InspectorObject> object = value->asObject();
    if (!object)
        return false;

    if (!object->getString("method", result))
        return false;

    return true;
}


} // namespace WebCore

#endif // ENABLE(INSPECTOR)
""")

    frontend_cpp = string.Template(file_header_ +
"""

#include "config.h"
#include "InspectorFrontend.h"
#include <wtf/text/WTFString.h>
#include <wtf/text/CString.h>

#if ENABLE(INSPECTOR)

#include "InspectorFrontendChannel.h"
#include "InspectorValues.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

InspectorFrontend::InspectorFrontend(InspectorFrontendChannel* inspectorFrontendChannel)
    : m_inspectorFrontendChannel(inspectorFrontendChannel)
$constructorInit{
}

$methods

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
""")

    backend_js = string.Template(file_header_ +
"""

$domainInitializers
""")

    param_container_access_code = """
    RefPtr<InspectorObject> paramsContainer = requestMessageObject->getObject("params");
    InspectorObject* paramsContainerPtr = paramsContainer.get();
    InspectorArray* protocolErrorsPtr = protocolErrors.get();
"""


type_map = TypeMap(json_api)

class Generator:
    frontend_class_field_lines = []
    frontend_domain_class_lines = []

    method_name_enum_list = []
    backend_method_declaration_list = []
    backend_method_implementation_list = []
    backend_method_name_declaration_list = []
    method_handler_list = []
    frontend_method_list = []
    backend_js_domain_initializer_list = []

    backend_setters_list = []
    backend_constructor_init_list = []
    backend_field_list = []
    backend_forward_list = []
    backend_include_list = []
    frontend_constructor_init_list = []
    type_builder_fragments = []

    @staticmethod
    def go():
        Generator.process_types(type_map)

        first_cycle_guardable_list_list = [
            Generator.backend_method_declaration_list,
            Generator.backend_method_implementation_list,
            Generator.backend_method_name_declaration_list,
            Generator.frontend_class_field_lines,
            Generator.frontend_constructor_init_list,
            Generator.frontend_domain_class_lines,
            Generator.frontend_method_list,
            Generator.method_handler_list,
            Generator.method_name_enum_list]

        for json_domain in json_api["domains"]:
            domain_name = json_domain["domain"]
            domain_name_lower = domain_name.lower()

            domain_fixes = DomainNameFixes.get_fixed_data(domain_name)

            domain_guard = domain_fixes.get_guard()

            if domain_guard:
                for l in first_cycle_guardable_list_list:
                    domain_guard.generate_open(l)

            agent_field_name = domain_fixes.agent_field_name

            frontend_method_declaration_lines = []

            Generator.backend_js_domain_initializer_list.append("// %s.\n" % domain_name)

            if not domain_fixes.skip_js_bind:
                Generator.backend_js_domain_initializer_list.append("InspectorBackend.register%sDispatcher = InspectorBackend.registerDomainDispatcher.bind(InspectorBackend, \"%s\");\n" % (domain_name, domain_name))

            if "events" in json_domain:
                for json_event in json_domain["events"]:
                    Generator.process_event(json_event, domain_name, frontend_method_declaration_lines)

            Generator.frontend_class_field_lines.append("    %s m_%s;\n" % (domain_name, domain_name_lower))
            Generator.frontend_constructor_init_list.append("    , m_%s(inspectorFrontendChannel)\n" % domain_name_lower)
            Generator.frontend_domain_class_lines.append(Templates.frontend_domain_class.substitute(None,
                domainClassName=domain_name,
                domainFieldName=domain_name_lower,
                frontendDomainMethodDeclarations=join(frontend_method_declaration_lines, "")))

            if "commands" in json_domain:
                for json_command in json_domain["commands"]:
                    Generator.process_command(json_command, domain_name, agent_field_name)

            if domain_guard:
                for l in reversed(first_cycle_guardable_list_list):
                    domain_guard.generate_close(l)
            Generator.backend_js_domain_initializer_list.append("\n")

        sorted_json_domains = list(json_api["domains"])
        sorted_json_domains.sort(key=lambda o: o["domain"])

        sorted_cycle_guardable_list_list = [
            Generator.backend_constructor_init_list,
            Generator.backend_setters_list,
            Generator.backend_field_list,
            Generator.backend_forward_list,
            Generator.backend_include_list]

        for json_domain in sorted_json_domains:
            domain_name = json_domain["domain"]

            domain_fixes = DomainNameFixes.get_fixed_data(domain_name)
            domain_guard = domain_fixes.get_guard()

            if domain_guard:
                for l in sorted_cycle_guardable_list_list:
                    domain_guard.generate_open(l)

            agent_type_name = domain_fixes.agent_type_name
            agent_field_name = domain_fixes.agent_field_name
            Generator.backend_constructor_init_list.append("        , m_%s(0)" % agent_field_name)
            Generator.backend_setters_list.append("    void registerAgent(%s* %s) { ASSERT(!m_%s); m_%s = %s; }" % (agent_type_name, agent_field_name, agent_field_name, agent_field_name, agent_field_name))
            Generator.backend_field_list.append("    %s* m_%s;" % (agent_type_name, agent_field_name))
            Generator.backend_forward_list.append("class %s;" % agent_type_name)
            Generator.backend_include_list.append("#include \"%s.h\"" % agent_type_name)

            if domain_guard:
                for l in reversed(sorted_cycle_guardable_list_list):
                    domain_guard.generate_close(l)

    @staticmethod
    def process_event(json_event, domain_name, frontend_method_declaration_lines):
        event_name = json_event["name"]
        parameter_list = []
        method_line_list = []
        backend_js_event_param_list = []
        if "parameters" in json_event:
            method_line_list.append("    RefPtr<InspectorObject> paramsObject = InspectorObject::create();\n")
            for json_parameter in json_event["parameters"]:
                parameter_name = json_parameter["name"]

                raw_type = resolve_param_raw_type(json_parameter, domain_name)

                json_optional = "optional" in json_parameter and json_parameter["optional"]

                optional_mask = raw_type.is_event_param_check_optional()
                c_type = raw_type.get_c_param_type(ParamType.EVENT, json_optional)

                setter_type = raw_type.get_setter_name()

                optional = optional_mask and json_optional

                parameter_list.append("%s %s" % (c_type.get_text(), parameter_name))

                setter_argument = c_type.get_setter_format() % parameter_name

                setter_code = "    paramsObject->set%s(\"%s\", %s);\n" % (setter_type, parameter_name, setter_argument)
                if optional:
                    setter_code = ("    if (%s)\n    " % parameter_name) + setter_code
                method_line_list.append(setter_code)

                backend_js_event_param_list.append("\"%s\"" % parameter_name)
            method_line_list.append("    %sMessage->setObject(\"params\", paramsObject);\n" % event_name)
        frontend_method_declaration_lines.append(
            "        void %s(%s);\n" % (event_name, join(parameter_list, ", ")))

        Generator.frontend_method_list.append(Templates.frontend_method.substitute(None,
            domainName=domain_name, eventName=event_name,
            parameters=join(parameter_list, ", "),
            code=join(method_line_list, "")))

        Generator.backend_js_domain_initializer_list.append("InspectorBackend.registerEvent(\"%s.%s\", [%s]);\n" % (
            domain_name, event_name, join(backend_js_event_param_list, ", ")))

    @staticmethod
    def process_command(json_command, domain_name, agent_field_name):
        json_command_name = json_command["name"]
        Generator.method_name_enum_list.append("        k%s_%sCmd," % (domain_name, json_command["name"]))
        Generator.method_handler_list.append("            &InspectorBackendDispatcher::%s_%s," % (domain_name, json_command_name))
        Generator.backend_method_declaration_list.append("    void %s_%s(long callId, InspectorObject* requestMessageObject);" % (domain_name, json_command_name))

        method_in_code = ""
        method_out_code = ""
        agent_call_param_list = []
        response_cook_list = []
        backend_js_reply_param_list = []
        request_message_param = ""
        js_parameters_text = ""
        if "parameters" in json_command:
            json_params = json_command["parameters"]
            method_in_code += Templates.param_container_access_code
            request_message_param = " requestMessageObject"
            js_param_list = []

            for json_parameter in json_params:
                json_param_name = json_parameter["name"]
                param_raw_type = resolve_param_raw_type(json_parameter, domain_name)

                var_type = param_raw_type.get_c_param_type(ParamType.INPUT, None)
                getter_name = param_raw_type.get_getter_name()

                if "optional" in json_parameter and json_parameter["optional"]:
                    code = ("    bool %s_valueFound = false;\n"
                            "    %s in_%s = get%s(paramsContainerPtr, \"%s\", &%s_valueFound, protocolErrorsPtr);\n" %
                           (json_param_name, var_type.get_text(), json_param_name, getter_name, json_param_name, json_param_name))
                    param = ", %s_valueFound ? &in_%s : 0" % (json_param_name, json_param_name)
                else:
                    code = ("    %s in_%s = get%s(paramsContainerPtr, \"%s\", 0, protocolErrorsPtr);\n" %
                            (var_type.get_text(), json_param_name, getter_name, json_param_name))
                    param = ", in_%s" % json_param_name

                method_in_code += code
                agent_call_param_list.append(param)

                js_bind_type = param_raw_type.get_js_bind_type()
                js_param_text = "{\"name\": \"%s\", \"type\": \"%s\", \"optional\": %s}" % (
                    json_param_name,
                    js_bind_type,
                    ("true" if ("optional" in json_parameter and json_parameter["optional"]) else "false"))

                js_param_list.append(js_param_text)

            js_parameters_text = join(js_param_list, ", ")

        response_cook_text = ""
        js_reply_list = "[]"
        if "returns" in json_command:
            method_out_code += "\n"
            for json_return in json_command["returns"]:

                json_return_name = json_return["name"]
                raw_type = resolve_param_raw_type(json_return, domain_name)
                setter_type = raw_type.get_setter_name()
                initializer = raw_type.get_c_initializer()
                var_type = raw_type.get_c_param_type(ParamType.OUTPUT, None)

                code = "    %s out_%s = %s;\n" % (var_type.get_text(), json_return_name, initializer)
                param = ", &out_%s" % json_return_name
                cook = "        result->set%s(\"%s\", out_%s);\n" % (setter_type, json_return_name, json_return_name)
                if var_type.get_text() == "bool" and "optional" in json_return and json_return["optional"]:
                    cook = ("        if (out_%s)\n    " % json_return_name) + cook

                method_out_code += code
                agent_call_param_list.append(param)
                response_cook_list.append(cook)

                backend_js_reply_param_list.append("\"%s\"" % json_return_name)

            js_reply_list = "[%s]" % join(backend_js_reply_param_list, ", ")

            response_cook_text = "    if (!protocolErrors->length() && !error.length()) {\n%s    }\n" % join(response_cook_list, "")

        Generator.backend_method_implementation_list.append(Templates.backend_method.substitute(None,
            domainName=domain_name, methodName=json_command_name,
            agentField="m_" + agent_field_name,
            methodInCode=method_in_code,
            methodOutCode=method_out_code,
            agentCallParams=join(agent_call_param_list, ""),
            requestMessageObject=request_message_param,
            responseCook=response_cook_text))
        Generator.backend_method_name_declaration_list.append("    \"%s.%s\"," % (domain_name, json_command_name))

        Generator.backend_js_domain_initializer_list.append("InspectorBackend.registerCommand(\"%s.%s\", [%s], %s);\n" % (domain_name, json_command_name, js_parameters_text, js_reply_list))

    @staticmethod
    def process_types(type_map):
        output = Generator.type_builder_fragments

        class ForwardListener:
            pass

        for domain_data in type_map.domains():

            domain_fixes = DomainNameFixes.get_fixed_data(domain_data.name())
            domain_guard = domain_fixes.get_guard()

            if domain_guard:
                domain_guard.generate_open(output)

            output.append("namespace ")
            output.append(domain_data.name())
            output.append(" {\n")
            for type_data in domain_data.types():
                type_data.get_binding().generate_type_builder(output, ForwardListener)

            output.append("} // ")
            output.append(domain_data.name())
            output.append("\n\n")

            if domain_guard:
                domain_guard.generate_close(output)

Generator.go()

backend_h_file = open(output_header_dirname + "/InspectorBackendDispatcher.h", "w")
backend_cpp_file = open(output_cpp_dirname + "/InspectorBackendDispatcher.cpp", "w")

frontend_h_file = open(output_header_dirname + "/InspectorFrontend.h", "w")
frontend_cpp_file = open(output_cpp_dirname + "/InspectorFrontend.cpp", "w")

backend_js_file = open(output_cpp_dirname + "/InspectorBackendStub.js", "w")


frontend_h_file.write(Templates.frontend_h.substitute(None,
         fieldDeclarations=join(Generator.frontend_class_field_lines, ""),
         domainClassList=join(Generator.frontend_domain_class_lines, ""),
         typeBuilders=join(Generator.type_builder_fragments, "")))

backend_h_file.write(Templates.backend_h.substitute(None,
    constructorInit=join(Generator.backend_constructor_init_list, "\n"),
    setters=join(Generator.backend_setters_list, "\n"),
    methodNamesEnumContent=join(Generator.method_name_enum_list, "\n"),
    methodDeclarations=join(Generator.backend_method_declaration_list, "\n"),
    fieldDeclarations=join(Generator.backend_field_list, "\n"),
    forwardDeclarations=join(Generator.backend_forward_list, "\n")))

frontend_cpp_file.write(Templates.frontend_cpp.substitute(None,
    constructorInit=join(Generator.frontend_constructor_init_list, ""),
    methods=join(Generator.frontend_method_list, "\n")))

backend_cpp_file.write(Templates.backend_cpp.substitute(None,
    methodNameDeclarations=join(Generator.backend_method_name_declaration_list, "\n"),
    includes=join(Generator.backend_include_list, "\n"),
    methods=join(Generator.backend_method_implementation_list, "\n"),
    messageHandlers=join(Generator.method_handler_list, "\n")))

backend_js_file.write(Templates.backend_js.substitute(None,
    domainInitializers=join(Generator.backend_js_domain_initializer_list, "")))

backend_h_file.close()
backend_cpp_file.close()

frontend_h_file.close()
frontend_cpp_file.close()

backend_js_file.close()
