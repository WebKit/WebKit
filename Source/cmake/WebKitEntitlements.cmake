# Wrapper to run the Xcode-based `process-additional-entitlements.sh` script
# during the build.
# 
# usage: WEBKIT_GENERATE_ENTITLEMENTS(<target>
#   USING <path>                           # path to process-entitlements.sh script
#   [BUNDLE_IDENTIFIER <bundle id>]         # if different from target name
#   [PRODUCT_NAME <product name>]           # if different from bundle identifier
#   [VARIANT <variant>]                     # XPC service variant to base extra entitlements off of
#   [NO_RESTRICTED_ENTITLEMENTS]            # omit restricted entitlements even if USE_RESTRICTED_ENTITLEMENTS
#   [OUTPUT <output path>]                  # if unspecified, a default will be used and set as <target>'s CODE_SIGN_ENTITLEMENTS path
#   [DEPENDS <file>...]                     # other files the script reads
# )

function(WEBKIT_GENERATE_ENTITLEMENTS _target)
    cmake_parse_arguments(_arg "EXTENSION;NO_RESTRICTED_ENTITLEMENTS" "PRODUCT_NAME;BUNDLE_IDENTIFIER;USING;OUTPUT;VARIANT" "DEPENDS" ${ARGN})
    if (NOT _arg_OUTPUT)
        set(_arg_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_target}.entitlements)
        set_target_properties(${_target} PROPERTIES CODE_SIGN_ENTITLEMENTS ${_arg_OUTPUT})
    endif ()
    if (NOT _arg_BUNDLE_IDENTIFIER)
        set(_arg_BUNDLE_IDENTIFIER ${_target})
    endif ()
    if (NOT _arg_PRODUCT_NAME)
        set(_arg_PRODUCT_NAME ${_arg_BUNDLE_IDENTIFIER})
    endif ()

    string(REPLACE "." ";" _version_components ${WEBKIT_SDK_VERSION})
    list(GET _version_components 0 _version_major)
    list(GET _version_components 1 _version_minor)
    math(EXPR _target_version_major "${_version_major} * 10000")
    math(EXPR _target_version_actual "(${_version_major} * 10000) + (${_version_minor} * 100)")

    set(_script ${_arg_USING})
    if (USE_APPLE_INTERNAL_SDK)
        set(_additional_entitlements_script ${WebKitAdditions_HEADERS_DIR}/Scripts/process-additional-entitlements.sh)
    endif ()

    if (USE_RESTRICTED_ENTITLEMENTS AND NOT _arg_NO_RESTRICTED_ENTITLEMENTS)
        set(_use_restricted_entitlements YES)
    else ()
        set(_use_restricted_entitlements NO)
    endif ()

    set(_skip_rosetta_breaking_entitlements "")
    if (CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
        set(_skip_rosetta_breaking_entitlements 1)
    endif ()
    add_custom_command(
        OUTPUT ${_arg_OUTPUT}
        COMMAND env
            BUILT_PRODUCTS_DIR=${CMAKE_BINARY_DIR}
            CONFIGURATION=${CMAKE_BUILD_TYPE}
            PLATFORM_NAME=${WEBKIT_SDK_NAME}
            PRODUCT_BUNDLE_IDENTIFIER=${_arg_BUNDLE_IDENTIFIER}
            PRODUCT_NAME=${_arg_PRODUCT_NAME}
            RC_XBS=
            SDKROOT=${CMAKE_OSX_SYSROOT}
            SDK_VERSION_ACTUAL=${_target_version_actual}
            # Checked by JSC's script, no longer set by the project.
            SKIP_ROSETTA_BREAKING_ENTITLEMENTS=${_skip_rosetta_breaking_entitlements}
            TARGET_MAC_OS_X_VERSION_MAJOR=${_target_version_major}
            WK_PLATFORM_NAME=${WEBKIT_SDK_NAME}
            WK_PROCESSED_XCENT_FILE=${_arg_OUTPUT}
            WK_RELOCATABLE_WEBPUSHD=$<IF:$<BOOL:${USE_RELOCATABLE_WEBPUSHD}>,YES,NO>
            WK_USE_FATAL_EXCEPTIONS=$<IF:$<BOOL:${USE_FATAL_EXCEPTIONS}>,YES,NO>
            WK_USE_RESTRICTED_ENTITLEMENTS=${_use_restricted_entitlements}
            WK_WEBCONTENT_SERVICE_NEEDS_XPC_DOMAIN_EXTENSION_ENTITLEMENT=$<IF:$<BOOL:${WEBCONTENT_SERVICE_NEEDS_XPC_DOMAIN_EXTENSION_ENTITLEMENT}>,YES,NO>
            WK_XPC_SERVICE_VARIANT=${_arg_VARIANT}
            # -eu flag to fail on build settings which need to be added to this
            # `env` invocation.
            sh -eu ${_script}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS ${_script} ${_additional_entitlements_script} ${_arg_DEPENDS}
        VERBATIM
    )
    add_custom_target(${_target}Entitlements DEPENDS ${_arg_OUTPUT})
    add_dependencies(${_target} ${_target}Entitlements)
endfunction()

# Embeds <xml_path> in <target>'s __TEXT,__entitlements and its DER encoding in
# __TEXT,__ents_der, as simulator binaries require. The target relinks when the
# entitlements change.
function(WEBKIT_EMBED_ENTITLEMENTS _target _xml_path)
    set(_der_output "${CMAKE_CURRENT_BINARY_DIR}/${_target}.entitlements.der")
    add_custom_command(
        OUTPUT "${_der_output}"
        COMMAND derq query -f xml -i "${_xml_path}" -o "${_der_output}" --raw
        DEPENDS "${_xml_path}"
        VERBATIM
    )
    target_sources(${_target} PRIVATE "${_der_output}")
    target_link_options(${_target} PRIVATE
        "LINKER:-sectcreate,__TEXT,__entitlements,${_xml_path}"
        "LINKER:-sectcreate,__TEXT,__ents_der,${_der_output}")
    set_property(TARGET ${_target} APPEND PROPERTY LINK_DEPENDS "${_xml_path}" "${_der_output}")
endfunction()

# Writes the get-task-allow entitlements used to sign simulator binaries.
function(WEBKIT_WRITE_SIMULATOR_SIGNING_ENTITLEMENTS _output)
    string(CONCAT _content
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\">\n"
        "<dict>\n"
        "\t<key>com.apple.security.get-task-allow</key>\n"
        "\t<true/>\n"
        "</dict>\n"
        "</plist>\n"
    )
    # Only writes when the content changes, so the signed targets don't relink on every configure.
    file(CONFIGURE OUTPUT ${_output} CONTENT "${_content}")
endfunction()
