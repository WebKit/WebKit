#!/usr/bin/env python3

import importlib
import re
import sys

log_declarations_module = importlib.import_module("generate-log-declarations")


def generate_messages_file(log_messages, log_messages_receiver_input_file, streaming_ipc):
    with open(log_messages_receiver_input_file, 'w') as file:
        file.write("#if ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)\n")
        file.write("[\n")
        file.write("    DispatchedFrom=WebContent,\n")
        file.write("    DispatchedTo=UI,\n")
        file.write("    ExceptionForEnabledBy\n")
        file.write("]\n")
        file.write("messages -> LogStream ")
        if streaming_ipc:
            file.write("Stream ")
        file.write("{\n")
        file.write("    LogOnBehalfOfWebContent(std::span<const uint8_t> logChannel, std::span<const uint8_t> logCategory, std::span<const uint8_t> logString, uint8_t logType)\n")
        for log_message in log_messages:
            message_name = log_message[0]
            parameters = log_message[2]
            arguments_string = log_declarations_module.get_arguments_string(parameters, log_declarations_module.PARAMETER_LIST_INCLUDE_TYPE | log_declarations_module.PARAMETER_LIST_INCLUDE_NAME)
            file.write("    " + message_name + "(" + arguments_string + ")\n")
        file.write("}\n")
        file.write("#endif\n")
        file.close()

    return


def generate_log_client_declarations_file(log_messages, log_client_declarations_file):
    with open(log_client_declarations_file, 'w') as file:

        for log_message in log_messages:
            function_name = log_message[0]
            parameters = log_message[2]
            arguments_string = log_declarations_module.get_arguments_string(parameters, log_declarations_module.PARAMETER_LIST_INCLUDE_TYPE | log_declarations_module.PARAMETER_LIST_INCLUDE_NAME | log_declarations_module.PARAMETER_LIST_MODIFY_CSTRING)
            file.write("    void " + function_name + "(" + arguments_string + ")\n")
            file.write("    {\n")
            parameters_string = log_declarations_module.get_arguments_string(parameters, log_declarations_module.PARAMETER_LIST_INCLUDE_NAME)
            file.write("        send(Messages::LogStream::" + function_name + "(" + parameters_string + "));\n")
            file.write("    }\n")
        file.close()

    return


def generate_message_receiver_declarations_file(log_messages, log_messages_receiver_declarations_file):
    with open(log_messages_receiver_declarations_file, 'w') as file:
        for log_message in log_messages:
            function_name = log_message[0]
            function_name = function_name[0].lower() + function_name[1:]
            parameters = log_message[2]
            arguments_string = log_declarations_module.get_arguments_string(parameters, log_declarations_module.PARAMETER_LIST_INCLUDE_TYPE | log_declarations_module.PARAMETER_LIST_INCLUDE_NAME | log_declarations_module.PARAMETER_LIST_MODIFY_CSTRING)
            file.write("    void " + function_name + "(" + arguments_string + ");\n")
        file.close()

    return


def generate_message_receiver_implementations_file(log_messages, log_messages_receiver_implementations_file):
    with open(log_messages_receiver_implementations_file, 'w') as file:
        for log_message in log_messages:
            function_name = log_message[0]
            function_name = function_name[0].lower() + function_name[1:]
            format_string = log_message[1]
            parameters = log_message[2]
            log_type = log_message[3]
            category = log_message[4]

            if log_type == "DEFAULT":
                os_log_type = "OS_LOG_TYPE_DEFAULT"
            elif log_type == "ERROR":
                os_log_type = "OS_LOG_TYPE_ERROR"
            elif log_type == "INFO":
                os_log_type = "OS_LOG_TYPE_INFO"
            elif log_type == "FAULT":
                os_log_type = "OS_LOG_TYPE_FAULT"

            file.write("void LogStream::" + function_name + "(")

            file.write(log_declarations_module.get_arguments_string(parameters, log_declarations_module.PARAMETER_LIST_INCLUDE_TYPE | log_declarations_module.PARAMETER_LIST_INCLUDE_NAME | log_declarations_module.PARAMETER_LIST_MODIFY_CSTRING))

            file.write(")\n")
            file.write("{\n")

            if category == "Testing":
                file.write("    globalLogCountForTesting++;\n")

            if category == "Default":
                file.write("    OSObjectPtr osLog = OS_LOG_DEFAULT;\n")
            else:
                file.write("    OSObjectPtr osLog = adoptOSObject(os_log_create(\"com.apple.WebKit\", \"" + category + "\"));\n")

            file.write("WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN\n")
            file.write("    os_log_with_type(osLog.get(), " + os_log_type + ", \"WebContent[%d]: \"" + format_string)
            file.write(", static_cast<uint32_t>(m_pid)")
            arguments_string = log_declarations_module.get_arguments_string(parameters, log_declarations_module.PARAMETER_LIST_INCLUDE_NAME | log_declarations_module.PARAMETER_LIST_MODIFY_CSTRING)
            if arguments_string:
                file.write(", ")
            file.write(arguments_string)
            file.write(");\n")
            file.write("WTF_ALLOW_UNSAFE_BUFFER_USAGE_END\n")
            file.write("}\n\n")
        file.close()

    return


def main(argv):

    webkit_log_messages_input_file = sys.argv[1]
    webcore_log_messages_input_file = sys.argv[2]
    log_messages_receiver_input_file = sys.argv[3]
    message_receiver_declarations_file = sys.argv[4]
    message_receiver_implementations_file = sys.argv[5]
    webkit_log_client_declarations_file = sys.argv[6]
    webcore_log_client_declarations_file = sys.argv[7]
    defines = sys.argv[8]

    streaming_ipc = defines.find("ENABLE_STREAMING_IPC_IN_LOG_FORWARDING") != -1

    webkit_log_messages = log_declarations_module.get_log_messages(webkit_log_messages_input_file)
    webcore_log_messages = log_declarations_module.get_log_messages(webcore_log_messages_input_file)

    log_messages = webkit_log_messages + webcore_log_messages

    generate_messages_file(log_messages, log_messages_receiver_input_file, streaming_ipc)

    generate_message_receiver_declarations_file(log_messages, message_receiver_declarations_file)
    generate_message_receiver_implementations_file(log_messages, message_receiver_implementations_file)

    generate_log_client_declarations_file(webkit_log_messages, webkit_log_client_declarations_file)
    generate_log_client_declarations_file(webcore_log_messages, webcore_log_client_declarations_file)

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
