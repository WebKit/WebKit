# Wrapper to run the Xcode-based `process-additional-entitlements.sh` script
# during the build.
# 
# usage: WEBKIT_GENERATE_ENTITLEMENTS(<target>
#   USING <path>                           # path to process-entitlements.sh script
#   [BUNDLE_IDENTIFIER <bundle id>]         # if different from target name
#   [PRODUCT_NAME <product name>]           # if different from bundle identifier
#   [VARIANT <variant>]                     # XPC service variant to base extra entitlements off of
#   [OUTPUT <output path>]                  # if unspecified, a default will be used and set as <target>'s CODE_SIGN_ENTITLEMENTS path
# )

function(WEBKIT_GENERATE_ENTITLEMENTS _target)
    cmake_parse_arguments(_arg "EXTENSION" "PRODUCT_NAME;BUNDLE_IDENTIFIER;USING;OUTPUT;VARIANT" "" ${ARGN})
    if (NOT _arg_OUTPUT)
        set(_arg_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_target}.entitlements)
        set(${_target}_CODE_SIGN_ENTITLEMENTS ${_arg_OUTPUT} PARENT_SCOPE)
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
            SKIP_ROSETTA_BREAKING_ENTITLEMENTS=
            TARGET_MAC_OS_X_VERSION_MAJOR=${_target_version_major}
            WK_PLATFORM_NAME=${WEBKIT_SDK_NAME}
            WK_PROCESSED_XCENT_FILE=${_arg_OUTPUT}
            WK_RELOCATABLE_WEBPUSHD=$<IF:$<BOOL:${USE_RELOCATABLE_WEBPUSHD}>,YES,NO>
            WK_USE_FATAL_EXCEPTIONS=$<IF:$<BOOL:${USE_FATAL_EXCEPTIONS}>,YES,NO>
            WK_USE_RESTRICTED_ENTITLEMENTS=$<IF:$<BOOL:${USE_RESTRICTED_ENTITLEMENTS}>,YES,NO>
            WK_WEBCONTENT_SERVICE_NEEDS_XPC_DOMAIN_EXTENSION_ENTITLEMENT=$<IF:$<BOOL:${WEBCONTENT_SERVICE_NEEDS_XPC_DOMAIN_EXTENSION_ENTITLEMENT}>,YES,NO>
            WK_XPC_SERVICE_VARIANT=${_arg_USING}
            # -eu flag to fail on build settings which need to be added to this
            # `env` invocation.
            sh -eu ${_script}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS ${_script} ${_additional_entitlements_script}
        VERBATIM
    )
    add_custom_target(${_target}Entitlements DEPENDS ${_arg_OUTPUT})
    add_dependencies(${_target} ${_target}Entitlements)
endfunction()
