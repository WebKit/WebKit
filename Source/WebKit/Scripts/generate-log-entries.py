#!/usr/bin/env python3

import re
import sys


def generate_messages_file(log_entries, log_entries_messages_file):
    print("Log entries messages file:", log_entries_messages_file)

    with open(log_entries_messages_file, 'w') as messages_file:
        messages_file.write("#if ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)\n")
        messages_file.write("messages -> LogStream NotRefCounted Stream {\n")
        messages_file.write("    LogOnBehalfOfWebContent(std::span<const uint8_t> logChannel, std::span<const uint8_t> logCategory, std::span<const uint8_t> logString, uint8_t logType)\n")
        for log_entry in log_entries:
            message_name = log_entry[0]
            parameters = log_entry[2]
            messages_file.write("    " + message_name + "(" + get_arguments_string(parameters, True) + ")\n")
        messages_file.write("}\n")
        messages_file.write("#endif\n")
        messages_file.close()

    return


def generate_message_receiver_declarations_file(log_entries, log_entries_message_receiver_declarations_file):
    print("Log entries message receiver header file:", log_entries_message_receiver_declarations_file)

    with open(log_entries_message_receiver_declarations_file, 'w') as message_receiver_declarations_file:
        for log_entry in log_entries:
            function_name = log_entry[0]
            function_name = function_name[0].lower() + function_name[1:]
            parameters = log_entry[2]
            message_receiver_declarations_file.write("    void " + function_name + "(" + parameters + ");\n")
        message_receiver_declarations_file.close()

    return


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


def generate_log_client_declarations_file(namespace, log_entries, log_client_declarations_file):
    print("Log entries message receiver header file:", log_client_declarations_file)

    namespace_camel_case = namespace[0].lower() + namespace[1:]

    with open(log_client_declarations_file, 'w') as file:

        file.write("virtual void " + namespace_camel_case + "Log(" + namespace + "::" + namespace + "LogMessage message, ...)\n")
        file.write("{\n")
        file.write("    va_list list;\n")
        file.write("    va_start(list, message);\n")
        file.write("    m_logStreamLock.lock();\n")
        file.write("    switch (message) {\n")
        for log_entry in log_entries:
            function_name = log_entry[0]
            parameters = log_entry[2]
            file.write("    case " + namespace + "::" + namespace + "LogMessage::" + function_name + ":\n")
            file.write("        m_logStreamConnection->send(Messages::LogStream::" + function_name + "(")
            arguments = re.findall(r'(\w+)\s*,?', parameters)
            for index, argument in enumerate(arguments):
                file.write("va_arg(list, " + argument + ")")
                if index < len(arguments) - 1:
                    file.write(", ")
            file.write("), m_logStreamIdentifier);\n")
            file.write("        break;\n")
        file.write("    }\n")
        file.write("    m_logStreamLock.unlock();\n")
        file.write("    va_end(list);\n")
        file.write("}\n")

#        for log_entry in log_entries:
#            function_name = log_entry[0]
#            parameters = log_entry[2]
#            file.write("    virtual void " + function_name + "(" + get_arguments_string(parameters, True) + ")\n")
#            file.write("    {\n")
#            file.write("        m_logStreamLock.lock();\n")
#            file.write("        m_logStreamConnection->send(Messages::LogStream::" + function_name + "(" + get_arguments_string(parameters, False) + "), m_logStreamIdentifier);\n")
#            file.write("        m_logStreamLock.unlock();\n")
#            file.write("    }\n")
#        file.close()

    return


def generate_message_receiver_implementations_file(log_entries, log_entries_message_receiver_implementations_file):
    print("Log entries message receiver implementations file:", log_entries_message_receiver_implementations_file)

    with open(log_entries_message_receiver_implementations_file, 'w') as message_receiver_implementations_file:
        for log_entry in log_entries:
            function_name = log_entry[0]
            function_name = function_name[0].lower() + function_name[1:]
            format_string = log_entry[1]
            parameters = log_entry[2]
            log_type = log_entry[3]
            category = log_entry[4]

            if log_type == "DEFAULT":
                os_log_type = "OS_LOG_TYPE_DEFAULT"
            elif log_type == "ERROR":
                os_log_type = "OS_LOG_TYPE_ERROR"
            elif log_type == "INFO":
                os_log_type = "OS_LOG_TYPE_INFO"
            elif log_type == "FAULT":
                os_log_type = "OS_LOG_TYPE_FAULT"

            message_receiver_implementations_file.write("void LogStream::" + function_name + "(")

            message_receiver_implementations_file.write(get_arguments_string(parameters, True))

            message_receiver_implementations_file.write(")\n")
            message_receiver_implementations_file.write("{\n")

            if category == "Default":
                message_receiver_implementations_file.write("    auto osLogPointer = OS_LOG_DEFAULT;\n")
            else:
                message_receiver_implementations_file.write("    auto osLog = adoptOSObject(os_log_create(\"com.apple.WebKit.WebContent\", \"" + category + "\"));\n")
                message_receiver_implementations_file.write("    auto osLogPointer = osLog.get();\n")

            message_receiver_implementations_file.write("    os_log_with_type(osLogPointer, " + os_log_type + ", \"[PID=%d]: \"" + format_string)
            message_receiver_implementations_file.write(", static_cast<uint32_t>(m_pid)")
            arguments_string = get_arguments_string(parameters, False)
            if arguments_string:
                message_receiver_implementations_file.write(", ")
            message_receiver_implementations_file.write(arguments_string)
            message_receiver_implementations_file.write(");\n")
            message_receiver_implementations_file.write("}\n\n")
        message_receiver_implementations_file.close()

    return


def get_log_entries(log_entries_input_file):
    log_entries = []
    with open(log_entries_input_file) as input_file:
        input_file_lines = input_file.readlines()
        for line in input_file_lines:
            match = re.search(r'([A-Z_0-9]*)\s*,\s*(\"[\w:;%\'\-\[\]=,\.\(\) ]*\")\s*,\s*\((.*)\)\s*,\s*(DEFAULT|INFO|ERROR|FAULT)\s*,\s*([\w]*)', line)
            log_entry = []
            if match:
                log_entry.append(match.groups()[0])
                log_entry.append(match.groups()[1])
                log_entry.append(match.groups()[2])
                log_entry.append(match.groups()[3])
                log_entry.append(match.groups()[4])
                log_entries.append(log_entry)
    return log_entries


def main(argv):

    webkit_log_entries_input_file = sys.argv[1]
    webcore_log_entries_input_file = sys.argv[2]
    log_entries_messages_file = sys.argv[3]
    message_receiver_declarations_file = sys.argv[4]
    message_receiver_implementations_file = sys.argv[5]
    webkit_log_client_declarations_file = sys.argv[6]
    webcore_log_client_declarations_file = sys.argv[7]

    print("WebKit Log entries input file:", webkit_log_entries_input_file)
    print("WebCore Log entries input file:", webcore_log_entries_input_file)

    webkit_log_entries = get_log_entries(webkit_log_entries_input_file)
    webcore_log_entries = get_log_entries(webcore_log_entries_input_file)

    log_entries = webkit_log_entries + webcore_log_entries

    generate_messages_file(log_entries, log_entries_messages_file)

    generate_message_receiver_declarations_file(log_entries, message_receiver_declarations_file)
    generate_message_receiver_implementations_file(log_entries, message_receiver_implementations_file)

    generate_log_client_declarations_file("WebKit", webkit_log_entries, webkit_log_client_declarations_file)
    generate_log_client_declarations_file("WebCore", webcore_log_entries, webcore_log_client_declarations_file)

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
