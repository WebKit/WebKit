#!/usr/bin/env python3

import re
import sys

PARAMETER_LIST_INCLUDE_TYPE = 1
PARAMETER_LIST_INCLUDE_NAME = 2
PARAMETER_LIST_MODIFY_CSTRING = 4


def get_argument_list(parameter_string):
    return re.findall(r'(\w+)\s*,?', parameter_string)


def get_arguments_string(parameter_string, flags):
    arguments = get_argument_list(parameter_string)
    arguments_string = ""
    for index, argument in enumerate(arguments):
        if flags & PARAMETER_LIST_INCLUDE_TYPE:
            if flags & PARAMETER_LIST_MODIFY_CSTRING and argument == "CString":
                argument = "CString&&"
            arguments_string += argument
        if flags & PARAMETER_LIST_INCLUDE_NAME:
            if flags & PARAMETER_LIST_INCLUDE_TYPE:
                arguments_string += " "
            arguments_string += "arg" + str(index)
            if (flags & PARAMETER_LIST_MODIFY_CSTRING) and (argument == "CString") and (not flags & PARAMETER_LIST_INCLUDE_TYPE):
                arguments_string += ".data()"
        if index < len(arguments) - 1:
            arguments_string += ", "
    return arguments_string


def generate_log_client_declarations_file(log_messages, log_client_declarations_file):
    with open(log_client_declarations_file, 'w') as file:

        file.write("#pragma once\n\n")
        for log_message in log_messages:
            file.write("#define MESSAGE_" + log_message[0] + " " + log_message[1] + "\n")

        file.close()

    return


def generate_log_client_virtual_functions(log_messages, log_client_virtual_functions_file):
    with open(log_client_virtual_functions_file, 'w') as file:
        file.write("""
/* Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <os/log.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class LogClient {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(LogClient);
public:
    LogClient() = default;
    virtual ~LogClient() { }

    virtual void log(std::span<const uint8_t> logChannel, std::span<const uint8_t> logCategory, std::span<const uint8_t> logString, os_log_type_t) = 0;
    virtual bool isWebKitLogClient() const { return false; }

""")

        for log_message in log_messages:
            function_name = log_message[0]
            parameters = log_message[2]
            arguments_string = get_arguments_string(parameters, PARAMETER_LIST_INCLUDE_TYPE | PARAMETER_LIST_MODIFY_CSTRING)
            file.write("    virtual void " + function_name + "(" + arguments_string + ") { }\n")

        file.write("""
    };

WEBCORE_EXPORT std::unique_ptr<LogClient>& logClient();

}
""")

        file.write("\n")

        file.close()

    return


def get_log_messages(log_messages_input_file):
    log_messages = []
    with open(log_messages_input_file) as input_file:
        input_file_lines = input_file.readlines()
        identifier_regexp = r'(?P<identifier>[A-Z_0-9]*)'
        inner_format_string_regexp = r'((\"[\w:;%~\'\-\[\]=,\.\(\)\{\} ]*\")\s*(PRI[A-Za-z0-9]+|PUBLIC_LOG_STRING|PRIVATE_LOG_STRING)?)'
        parameter_list_regexp = r'\((?P<parameter_list>.*)\)'
        log_type_regexp = r'(?P<log_type>DEFAULT|INFO|ERROR|FAULT)'
        log_category_regexp = r'(?P<category>[\w]*)'
        format_string_regexp = r'(?P<format_string>(' + inner_format_string_regexp + r'(\s*))+)'
        separator_regexp = r'\s*,\s*'
        for line in input_file_lines:
            match = re.search(identifier_regexp + separator_regexp + format_string_regexp + separator_regexp + parameter_list_regexp + separator_regexp + log_type_regexp + separator_regexp + log_category_regexp, line)
            log_message = []
            if match:
                log_message.append(match.group('identifier'))
                log_message.append(match.group('format_string'))
                log_message.append(match.group('parameter_list'))
                log_message.append(match.group('log_type'))
                log_message.append(match.group('category'))
                log_messages.append(log_message)
            elif line[0] != '#' and line[0] != '\n':
                print("Unable to match log message " + line)
                sys.exit(1)

    return log_messages


def main(argv):

    log_messages_input_file = sys.argv[1]
    log_messages_declarations_file = sys.argv[2]
    if len(sys.argv) > 3:
        log_client_virtual_functions_file = sys.argv[3]
    else:
        log_client_virtual_functions_file = None

    log_messages = get_log_messages(log_messages_input_file)

    generate_log_client_declarations_file(log_messages, log_messages_declarations_file)

    if log_client_virtual_functions_file is not None:
        generate_log_client_virtual_functions(log_messages, log_client_virtual_functions_file)

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
