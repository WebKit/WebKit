#!/usr/bin/env python3

import importlib
import re
import sys

log_declarations_module = importlib.import_module("generate-log-declarations")


def generate_messages_file(log_entries, log_entries_messages_file):
    print("Log entries messages file:", log_entries_messages_file)

    with open(log_entries_messages_file, 'w') as messages_file:
        messages_file.write("#if ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)\n")
        messages_file.write("messages -> LogStream NotRefCounted Stream {\n")
        messages_file.write("    LogOnBehalfOfWebContent(std::span<const uint8_t> logChannel, std::span<const uint8_t> logCategory, std::span<const uint8_t> logString, uint8_t logType)\n")
        for log_entry in log_entries:
            message_name = log_entry[0]
            parameters = log_entry[2]
            arguments_string = log_declarations_module.get_arguments_string(parameters, log_declarations_module.PARAMETER_LIST_INCLUDE_TYPE | log_declarations_module.PARAMETER_LIST_INCLUDE_NAME)
            messages_file.write("    " + message_name + "(" + arguments_string + ")\n")
        messages_file.write("}\n")
        messages_file.write("#endif\n")
        messages_file.close()

    return


def generate_log_client_declarations_file(namespace, log_entries, log_client_declarations_file):
    print("Log entries message receiver header file:", log_client_declarations_file)

    namespace_camel_case = namespace[0].lower() + namespace[1:]

    with open(log_client_declarations_file, 'w') as file:

        for log_entry in log_entries:
            function_name = log_entry[0]
            parameters = log_entry[2]
            arguments_string = log_declarations_module.get_arguments_string(parameters, log_declarations_module.PARAMETER_LIST_INCLUDE_TYPE | log_declarations_module.PARAMETER_LIST_INCLUDE_NAME | log_declarations_module.PARAMETER_LIST_MODIFY_CSTRING)
            file.write("    virtual void " + function_name + "(" + arguments_string + ")\n")
            file.write("    {\n")
            file.write("        m_logStreamLock.lock();\n")
            file.write("        if (RefPtr connection = logStreamConnection())\n")
            parameters_string = log_declarations_module.get_arguments_string(parameters, log_declarations_module.PARAMETER_LIST_INCLUDE_NAME)
            file.write("            connection->send(Messages::LogStream::" + function_name + "(" + parameters_string + "), m_logStreamIdentifier);\n")
            file.write("        m_logStreamLock.unlock();\n")
            file.write("    }\n")
        file.close()

    return


def generate_message_receiver_declarations_file(log_entries, log_entries_message_receiver_declarations_file):
    print("Log entries message receiver header file:", log_entries_message_receiver_declarations_file)

    with open(log_entries_message_receiver_declarations_file, 'w') as message_receiver_declarations_file:
        for log_entry in log_entries:
            function_name = log_entry[0]
            function_name = function_name[0].lower() + function_name[1:]
            parameters = log_entry[2]
            arguments_string = log_declarations_module.get_arguments_string(parameters, log_declarations_module.PARAMETER_LIST_INCLUDE_TYPE | log_declarations_module.PARAMETER_LIST_INCLUDE_NAME | log_declarations_module.PARAMETER_LIST_MODIFY_CSTRING)
            message_receiver_declarations_file.write("    void " + function_name + "(" + arguments_string + ");\n")
        message_receiver_declarations_file.close()

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

            message_receiver_implementations_file.write(log_declarations_module.get_arguments_string(parameters, log_declarations_module.PARAMETER_LIST_INCLUDE_TYPE | log_declarations_module.PARAMETER_LIST_INCLUDE_NAME | log_declarations_module.PARAMETER_LIST_MODIFY_CSTRING))

            message_receiver_implementations_file.write(")\n")
            message_receiver_implementations_file.write("{\n")

            if category == "Default":
                message_receiver_implementations_file.write("    auto osLogPointer = OS_LOG_DEFAULT;\n")
            else:
                message_receiver_implementations_file.write("    auto osLog = adoptOSObject(os_log_create(\"com.apple.WebKit.WebContent\", \"" + category + "\"));\n")
                message_receiver_implementations_file.write("    auto osLogPointer = osLog.get();\n")

            message_receiver_implementations_file.write("    os_log_with_type(osLogPointer, " + os_log_type + ", \"[PID=%d]: \"" + format_string)
            message_receiver_implementations_file.write(", static_cast<uint32_t>(m_pid)")
            arguments_string = log_declarations_module.get_arguments_string(parameters, log_declarations_module.PARAMETER_LIST_INCLUDE_NAME | log_declarations_module.PARAMETER_LIST_MODIFY_CSTRING)
            if arguments_string:
                message_receiver_implementations_file.write(", ")
            message_receiver_implementations_file.write(arguments_string)
            message_receiver_implementations_file.write(");\n")
            message_receiver_implementations_file.write("}\n\n")
        message_receiver_implementations_file.close()

    return


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

    webkit_log_entries = log_declarations_module.get_log_entries(webkit_log_entries_input_file)
    webcore_log_entries = log_declarations_module.get_log_entries(webcore_log_entries_input_file)

    log_entries = webkit_log_entries + webcore_log_entries

    generate_messages_file(log_entries, log_entries_messages_file)

    generate_message_receiver_declarations_file(log_entries, message_receiver_declarations_file)
    generate_message_receiver_implementations_file(log_entries, message_receiver_implementations_file)

    generate_log_client_declarations_file("WebKit", webkit_log_entries, webkit_log_client_declarations_file)
    generate_log_client_declarations_file("WebCore", webcore_log_entries, webcore_log_client_declarations_file)

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
