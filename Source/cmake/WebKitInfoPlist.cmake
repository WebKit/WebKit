# Processes an Info.plist at configure time, the way Xcode's ProcessInfoPlistFile
# build step does.
#
# usage: WEBKIT_PROCESS_INFO_PLIST(<input> <output>
#   [PREPROCESS]                                # run <input> through the C preprocessor first
#   [PREPROCESSOR_DEFINITIONS <definition>...]  # NAME or NAME=VALUE macros to define while preprocessing
#   [TARGETED_DEVICE_FAMILY <family>...]        # UIDeviceFamily values, narrowed to the SDK; defaults to the SDK's primary family
# )
#
# ${VAR} and @VAR@ references are substituted from the calling scope, as
# configure_file() does. Then it adds common platform keys set by
# WebKitXcodeSDK.
#
# <output> is only rewritten when its contents change, so reconfiguring doesn't
# relink or re-sign the bundle it belongs to.

function(WEBKIT_PROCESS_INFO_PLIST _input _output)
    cmake_parse_arguments(_arg "PREPROCESS" "" "PREPROCESSOR_DEFINITIONS;TARGETED_DEVICE_FAMILY" ${ARGN})
    if (_arg_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "WEBKIT_PROCESS_INFO_PLIST: unexpected arguments: ${_arg_UNPARSED_ARGUMENTS}")
    endif ()

    get_filename_component(_input ${_input} ABSOLUTE BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
    get_filename_component(_output ${_output} ABSOLUTE BASE_DIR ${CMAKE_CURRENT_BINARY_DIR})
    set(_script ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/process-info-plist)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_input} ${_script})

    file(READ ${_input} _contents)
    string(CONFIGURE "${_contents}" _contents)
    get_filename_component(_name ${_output} NAME)
    string(MD5 _hash ${_output})
    set(_configured ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/WebKitInfoPlist/${_hash}-${_name})
    file(WRITE ${_configured} "${_contents}")

    set(_options "")
    if (_arg_PREPROCESS)
        list(TRANSFORM _arg_PREPROCESSOR_DEFINITIONS PREPEND -D)
        list(APPEND _options --cpp ${CMAKE_C_COMPILER} ${_arg_PREPROCESSOR_DEFINITIONS})
    endif ()
    if (WEBKIT_SDK_IS_IOS_FAMILY)
        WEBKIT_GET_DEVICE_FAMILY(_device_family ${_arg_TARGETED_DEVICE_FAMILY})
        list(APPEND _options --device-family ${_device_family})
    endif ()
    execute_process(
        COMMAND ${Python_EXECUTABLE} ${_script} ${_configured} ${_output}
            --platform ${WEBKIT_PLATFORM_NAME}
            --sdk ${WEBKIT_SDK_NAME}
            --deployment-target ${CMAKE_OSX_DEPLOYMENT_TARGET}
            ${_options}
        COMMAND_ERROR_IS_FATAL ANY)
endfunction()
