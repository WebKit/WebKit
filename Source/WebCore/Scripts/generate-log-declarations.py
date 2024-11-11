#!/usr/bin/env python3

import re
import sys


def get_arguments_string(parameters, include_type):
    arguments = re.findall(r'(\w+)\s*,?', parameters)
    arguments_string = ""
    for index, argument in enumerate(arguments):
        if include_type:
            arguments_string += argument + " "
        arguments_string += "argument" + str(index)
        if index < len(arguments) - 1:
            arguments_string += ", "
    return arguments_string


def generate_log_client_declarations_file(namespace, log_messages, log_client_declarations_file):
    print("Log message receiver header file:", log_client_declarations_file)

    with open(log_client_declarations_file, 'w') as file:

        file.write("#pragma once\n\n")
        file.write("#if ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)\n")
        file.write("namespace " + namespace + " {\n")
        file.write("\n")
        file.write("enum class " + namespace + "LogMessage : uint32_t {\n")
        for log_message in log_messages:
            file.write("    " + log_message[0] + ",\n")
        file.write("};\n")
        file.write("\n")
        file.write("}\n")
        file.write("#endif\n")
        for log_message in log_messages:
            file.write("#define MESSAGE_" + log_message[0] + " " + log_message[1] + "\n")

        file.close()

    return


def get_log_entries(log_entries_input_file):
    log_entries = []
    with open(log_entries_input_file) as input_file:
        input_file_lines = input_file.readlines()
        identifier_regexp = '(?P<identifier>[A-Z_0-9]*)'
        inner_format_string_regexp = '(\"[\w:;%\'\-\[\]=,\.\(\)\{\} ]*\")|([A-Za-z]+)'
        parameter_list_regexp = '\((?P<parameter_list>.*)\)'
        log_type_regexp = '(?P<log_type>DEFAULT|INFO|ERROR|FAULT)'
        log_category_regexp = '(?P<category>[\w]*)'
        format_string_regexp = '(?P<format_string>' + inner_format_string_regexp + '(\s*' + inner_format_string_regexp + ')*)'
        for line in input_file_lines:
            match = re.search(identifier_regexp + '\s*,\s*' + format_string_regexp + '\s*,\s*' + parameter_list_regexp + '\s*,\s*' + log_type_regexp + '\s*,\s*' + log_category_regexp, line)
            log_entry = []
            if match:
                log_entry.append(match.group('identifier'))
                log_entry.append(match.group('format_string'))
                log_entry.append(match.group('parameter_list'))
                log_entry.append(match.group('log_type'))
                log_entry.append(match.group('category'))
                log_entries.append(log_entry)
            else:
                # FIXME: exit on error
                print("Unable to match format string " + line)

    return log_entries


def main(argv):

    namespace = sys.argv[1]
    log_entries_input_file = sys.argv[2]
    log_entries_declarations_file = sys.argv[3]

    print("Log entries input file:", log_entries_input_file)

    log_entries = get_log_entries(log_entries_input_file)

    generate_log_client_declarations_file(namespace, log_entries, log_entries_declarations_file)

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
