cmake_minimum_required(VERSION 3.22)

list(APPEND CMAKE_MESSAGE_CONTEXT "Corrosion")

message(DEBUG "Using Corrosion ${Corrosion_VERSION} with CMake ${CMAKE_VERSION} "
        "and the `${CMAKE_GENERATOR}` Generator"
)

get_cmake_property(COR_IS_MULTI_CONFIG GENERATOR_IS_MULTI_CONFIG)
set(COR_IS_MULTI_CONFIG "${COR_IS_MULTI_CONFIG}" CACHE BOOL "Do not change this" FORCE)
mark_as_advanced(FORCE COR_IS_MULTI_CONFIG)


if(NOT COR_IS_MULTI_CONFIG AND DEFINED CMAKE_CONFIGURATION_TYPES)
    message(WARNING "The Generator is ${CMAKE_GENERATOR}, which is not a multi-config "
        "Generator, but CMAKE_CONFIGURATION_TYPES is set. Please don't set "
        "CMAKE_CONFIGURATION_TYPES unless you are using a multi-config Generator."
    )
endif()

option(CORROSION_VERBOSE_OUTPUT "Enables verbose output from Corrosion and Cargo" OFF)

if(DEFINED CORROSION_RESPECT_OUTPUT_DIRECTORY AND NOT CORROSION_RESPECT_OUTPUT_DIRECTORY)
    message(WARNING "The option CORROSION_RESPECT_OUTPUT_DIRECTORY was removed."
    " Corrosion now always attempts to respect the output directory.")
endif()

option(
    CORROSION_NO_WARN_PARSE_TARGET_TRIPLE_FAILED
    "Surpresses a warning if the parsing the target triple failed."
    OFF
)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_SYSTEM_NAME STREQUAL "iOS")
    if(DEFINED CORROSION_HOST_TARGET_LINKER)
        set(_corrosion_host_linker "${CORROSION_HOST_TARGET_LINKER}")
        message(DEBUG "Using user provided CORROSION_HOST_TARGET_LINKER: ${CORROSION_HOST_TARGET_LINKER}")
    else()
        set(_corrosion_host_linker "/usr/bin/cc")
    endif()
    set(CORROSION_HOST_TARGET_LINKER "${_corrosion_host_linker}"
        CACHE STRING
        "The linker-driver corrosion will use to compile host-targets. Currently only used when cross-compiling for iOS."
        FORCE)
endif()

find_package(Rust REQUIRED)

if(CMAKE_GENERATOR MATCHES "Visual Studio"
        AND (NOT CMAKE_VS_PLATFORM_NAME STREQUAL CMAKE_VS_PLATFORM_NAME_DEFAULT)
        AND Rust_VERSION VERSION_LESS "1.54")
    message(FATAL_ERROR "Due to a cargo issue, cross-compiling with a Visual Studio generator and rust versions"
            " before 1.54 is not supported. Rust build scripts would be linked with the cross-compiler linker, which"
            " causes the build to fail. Please upgrade your Rust version to 1.54 or newer.")
endif()

#    message(STATUS "Using Corrosion as a subdirectory")

get_property(
    RUSTC_EXECUTABLE
    TARGET Rust::Rustc PROPERTY IMPORTED_LOCATION
)

get_property(
    CARGO_EXECUTABLE
    TARGET Rust::Cargo PROPERTY IMPORTED_LOCATION
)

if(Rust_TOOLCHAIN_IS_RUSTUP_MANAGED AND DEFINED Rust_RUSTUP_TOOLCHAINS)
    set(corrosion_tools_rust_toolchain_docstring "Rust toolchain to use for building helper tools such as cbindgen or cxx-bridge")
    if(DEFINED CORROSION_TOOLS_RUST_TOOLCHAIN)
        set(cor_default_tools_toolchain "${CORROSION_TOOLS_RUST_TOOLCHAIN}")
    else()
        set(cor_default_tools_toolchain "${Rust_TOOLCHAIN}")
    endif()
    set(CORROSION_TOOLS_RUST_TOOLCHAIN "${cor_default_tools_toolchain}" CACHE STRING
        "${corrosion_tools_rust_toolchain_docstring}" FORCE)
    set_property(CACHE CORROSION_TOOLS_RUST_TOOLCHAIN PROPERTY STRINGS "${Rust_RUSTUP_TOOLCHAINS}")
    if(NOT "$CACHE{CORROSION_TOOLS_RUST_TOOLCHAIN}" IN_LIST Rust_RUSTUP_TOOLCHAINS)
        if("$CACHE{CORROSION_TOOLS_RUST_TOOLCHAIN}-${Rust_CARGO_HOST_TARGET}" IN_LIST Rust_RUSTUP_TOOLCHAINS)
            set(CORROSION_TOOLS_RUST_TOOLCHAIN "$CACHE{CORROSION_TOOLS_RUST_TOOLCHAIN}-${Rust_CARGO_HOST_TARGET}"
                CACHE PATH "${corrosion_tools_rust_toolchain_docstring}" FORCE)
        else()
            message(FATAL_ERROR "CORROSION_TOOLS_RUST_TOOLCHAIN must be set to a valid rustup managed toolchain path."
                    "Rust_RUSTUP_TOOLCHAINS contains a list of valid installed toolchains."
            )
        endif()
    endif()
    foreach(toolchain tc_rustc tc_cargo IN ZIP_LISTS Rust_RUSTUP_TOOLCHAINS Rust_RUSTUP_TOOLCHAINS_RUSTC_PATH Rust_RUSTUP_TOOLCHAINS_CARGO_PATH)
        if("${toolchain}" STREQUAL $CACHE{CORROSION_TOOLS_RUST_TOOLCHAIN})
            # Minimum CMake version 3.29 for `IS_EXECUTABLE`.
            if(NOT (tc_cargo AND tc_rustc ))
                message(FATAL_ERROR "Failed to find executable rustc or cargo for toolchain `$CACHE{CORROSION_TOOLS_RUST_TOOLCHAIN}`")
            endif()
            set(CORROSION_TOOLS_RUSTC "${tc_rustc}" CACHE INTERNAL "" FORCE)
            set(CORROSION_TOOLS_CARGO "${tc_cargo}" CACHE INTERNAL "" FORCE)
            break()
        endif()
    endforeach()
    if(NOT DEFINED CACHE{CORROSION_TOOLS_CARGO})
        message(FATAL_ERROR "Internal error: Failed to find toolchain $CACHE{CORROSION_TOOLS_RUST_TOOLCHAIN} in "
                "list of rustup managed toolchains: ${Rust_RUSTUP_TOOLCHAINS}"
        )
    endif()
else()
    # Fallback to the default project toolchain if rust is not rustup managed.
    if(DEFINED CORROSION_TOOLS_RUST_TOOLCHAIN)
        message(DEBUG "Ignoring `CORROSION_TOOLS_RUST_TOOLCHAIN=${CORROSION_TOOLS_RUST_TOOLCHAIN}` "
            "since the toolchains are not rustup managed. Falling back to the default rust toolchain"
            " for this project."
        )
    endif()
    set(CORROSION_TOOLS_RUSTC "${RUSTC_EXECUTABLE}" CACHE INTERNAL "" FORCE)
    set(CORROSION_TOOLS_CARGO "${CARGO_EXECUTABLE}" CACHE INTERNAL "" FORCE)
endif()

function(_corrosion_bin_target_suffix target_name out_var_suffix)
    get_target_property(hostbuild "${target_name}" ${_CORR_PROP_HOST_BUILD})
    if((hostbuild AND CMAKE_HOST_WIN32)
       OR ((NOT hostbuild) AND (Rust_CARGO_TARGET_OS STREQUAL "windows")))
        set(_suffix ".exe")
    elseif(Rust_CARGO_TARGET_OS STREQUAL "vxworks")
        set(_suffix ".vxe")
    else()
        set(_suffix "")
    endif()
    set(${out_var_suffix} "${_suffix}" PARENT_SCOPE)
endfunction()

function(_handle_output_directory_genex input_path config_type output_path)
    if("${config_type}" STREQUAL "")
        # Prevent new path from being `dir//file`, since that causes issues with the
        # file dependency.
        string(REPLACE "/\$<CONFIG>" "${config_type}" curr_out_dir "${input_path}")
        string(REPLACE "\$<CONFIG>" "${config_type}" curr_out_dir "${curr_out_dir}")
    else()
        string(REPLACE "\$<CONFIG>" "${config_type}" curr_out_dir "${input_path}")
    endif()
    string(GENEX_STRIP "${curr_out_dir}" stripped_out_dir)
    if("${stripped_out_dir}" STREQUAL "${curr_out_dir}")
        set("${output_path}" "${curr_out_dir}" PARENT_SCOPE)
    else()
        unset("${output_path}" PARENT_SCOPE)
        message(WARNING "Encountered output directory path with unsupported genex. "
                "Output dir: `${curr_out_dir}`"
                "Note: Corrosion only supports the `\$<CONFIG>` generator expression for output directories.")
    endif()
endfunction()

# Do not call this function directly!
#
# This function should be called deferred to evaluate target properties late in the configure stage.
# IMPORTED_LOCATION does not support Generator expressions, so we must evaluate the output
# directory target property value at configure time. This function must be deferred to the end of
# the configure stage, so we can be sure that the output directory is not modified afterwards.
function(_corrosion_set_imported_location_deferred target_name base_property output_directory_property filename)
    # The output directory property is expected to be set on the exposed target (without postfix),
    # but we need to set the imported location on the actual library target with postfix.
    if("${target_name}" MATCHES "^(.+)-(static|shared)$")
        set(output_dir_prop_target_name "${CMAKE_MATCH_1}")
    else()
        set(output_dir_prop_target_name "${target_name}")
    endif()

    # Append .exe suffix for executable by-products if the target is windows or if it's a host
    # build and the host is Windows.
    get_target_property(target_type ${target_name} TYPE)
    if(${target_type} STREQUAL "EXECUTABLE" AND (NOT "${filename}" MATCHES "\.pdb$"))
        _corrosion_bin_target_suffix(${target_name} "suffix")
        string(APPEND filename "${suffix}")
    endif()

    get_target_property(output_directory "${output_dir_prop_target_name}" "${output_directory_property}")
    message(DEBUG "Output directory property (target ${output_dir_prop_target_name}): ${output_directory_property} dir: ${output_directory}")

    foreach(config_type ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER "${config_type}" config_type_upper)
        get_target_property(output_dir_curr_config ${output_dir_prop_target_name}
            "${output_directory_property}_${config_type_upper}"
        )
        if(output_dir_curr_config)
            set(curr_out_dir "${output_dir_curr_config}")
        elseif(output_directory)
            string(GENEX_STRIP "${output_directory}" output_dir_no_genex)
            # Only add config dir if there is no genex in here. See
            # https://cmake.org/cmake/help/latest/prop_tgt/RUNTIME_OUTPUT_DIRECTORY.html
            if(output_directory STREQUAL output_dir_no_genex)
                set(curr_out_dir "${output_directory}/${config_type}")
            else()
                set(curr_out_dir "${output_directory}")
            endif()
        else()
            set(curr_out_dir "${CMAKE_CURRENT_BINARY_DIR}")
        endif()
        _handle_output_directory_genex("${curr_out_dir}" "${config_type}" sanitized_out_dir)
        if(NOT DEFINED sanitized_out_dir)
            message(FATAL_ERROR "${output_directory_property} for target ${output_dir_prop_target_name} "
                    "contained an unexpected Generator expression. Output dir: `${curr_out_dir}`"
                "Note: Corrosion only supports the `\$<CONFIG>` generator expression for output directories.")
        endif()

        # For Multiconfig we want to specify the correct location for each configuration
        set_property(
            TARGET ${target_name}
            PROPERTY "${base_property}_${config_type_upper}"
                "${sanitized_out_dir}/${filename}"
        )
        set(base_output_directory "${sanitized_out_dir}")
    endforeach()

    if(NOT COR_IS_MULTI_CONFIG)
        if(output_directory)
            set(base_output_directory "${output_directory}")
        else()
            set(base_output_directory "${CMAKE_CURRENT_BINARY_DIR}")
        endif()
        _handle_output_directory_genex("${base_output_directory}" "${CMAKE_BUILD_TYPE}" sanitized_output_directory)
        if(NOT DEFINED sanitized_output_directory)
            message(FATAL_ERROR "${output_dir_prop_target_name} for target ${output_dir_prop_target_name} "
                    "contained an unexpected Generator expression. Output dir: `${base_output_directory}`."
                    "Note: Corrosion only supports the `\$<CONFIG>` generator expression for output directories.")
        endif()
        set(base_output_directory "${sanitized_output_directory}")
    endif()

    message(DEBUG "Setting ${base_property} for target ${target_name}"
                " to `${base_output_directory}/${filename}`.")

    # IMPORTED_LOCATION must be set regardless of possible overrides. In the multiconfig case,
    # the last configuration "wins" (IMPORTED_LOCATION is not documented to have Genex support).
    set_property(
            TARGET ${target_name}
            PROPERTY "${base_property}" "${base_output_directory}/${filename}"
        )
endfunction()

# Set the imported location of a Rust target.
#
# Rust targets are built via custom targets / custom commands. The actual artifacts are exposed
# to CMake as imported libraries / executables that depend on the cargo_build command. For CMake
# to find the built artifact we need to set the IMPORTED location to the actual location on disk.
# Corrosion tries to copy the artifacts built by cargo to standard locations. The IMPORTED_LOCATION
# is set to point to the copy, and not the original from the cargo build directory.
#
# Parameters:
# - target_name: Name of the Rust target
# - base_property: Name of the base property - i.e. `IMPORTED_LOCATION` or `IMPORTED_IMPLIB`.
# - output_directory_property: Target property name that determines the standard location for the
#    artifact.
# - filename of the artifact.
function(_corrosion_set_imported_location target_name base_property output_directory_property filename)
        cmake_language(EVAL CODE "
            cmake_language(DEFER
                CALL
                _corrosion_set_imported_location_deferred
                [[${target_name}]]
                [[${base_property}]]
                [[${output_directory_property}]]
                [[${filename}]]
            )
        ")
endfunction()

function(_corrosion_copy_byproduct_deferred target_name output_dir_prop_names cargo_build_dir file_names)
    if(ARGN)
        message(FATAL_ERROR "Unexpected additional arguments")
    endif()

    foreach(output_dir_prop_name ${output_dir_prop_names})
        get_target_property(output_dir ${target_name} "${output_dir_prop_name}")
        if(output_dir)
            break()
        endif()
    endforeach()

    # A Genex expanding to the output directory depending on the configuration.
    set(multiconfig_out_dir_genex "")

    foreach(config_type ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER "${config_type}" config_type_upper)
        foreach(output_dir_prop_name ${output_dir_prop_names})
            get_target_property(output_dir_curr_config ${target_name} "${output_dir_prop_name}_${config_type_upper}")
            if(output_dir_curr_config)
                break()
            endif()
        endforeach()

        if(output_dir_curr_config)
            set(curr_out_dir "${output_dir_curr_config}")
        elseif(output_dir)
            string(GENEX_STRIP "${output_dir}" output_dir_no_genex)
            # Only add config dir if there is no genex in here. See
            # https://cmake.org/cmake/help/latest/prop_tgt/RUNTIME_OUTPUT_DIRECTORY.html
            # Logic duplicated from _corrosion_set_imported_location_deferred
            if(output_dir STREQUAL output_dir_no_genex)
                set(curr_out_dir "${output_dir}/${config_type}")
            else()
                set(curr_out_dir "${output_dir}")
            endif()
        else()
            # Fallback to the default directory. We do not append the configuration directory here
            # and instead let CMake do this, since otherwise the resolving of dynamic library
            # imported paths may fail.
            set(curr_out_dir "${CMAKE_CURRENT_BINARY_DIR}")
        endif()
        set(multiconfig_out_dir_genex "${multiconfig_out_dir_genex}$<$<CONFIG:${config_type}>:${curr_out_dir}>")
    endforeach()

    if(COR_IS_MULTI_CONFIG)
        set(output_dir "${multiconfig_out_dir_genex}")
    else()
        if(NOT output_dir)
            # Fallback to default directory.
            set(output_dir "${CMAKE_CURRENT_BINARY_DIR}")
        endif()
    endif()

    # Append .exe suffix for executable by-products if the target is windows or if it's a host
    # build and the host is Windows.
    get_target_property(target_type "${target_name}" TYPE)
    if (target_type STREQUAL "EXECUTABLE")
        list(LENGTH file_names list_len)
        if(NOT list_len EQUAL "1")
            message(FATAL_ERROR
                    "Internal error: Exactly one filename should be passed for executable types.")
        endif()
        _corrosion_bin_target_suffix(${target_name} "suffix")
        if(suffix AND (NOT "${file_names}" MATCHES "\.pdb$"))
            # For executable targets we know / checked that only one file will be passed.
            string(APPEND file_names "${suffix}")
        endif()
    endif()
    set(src_file_names "${file_names}")
    if(Rust_CARGO_TARGET_ENV STREQUAL "gnullvm")
        # Workaround for cargo not exposing implibs yet.
        list(TRANSFORM src_file_names PREPEND "deps/" REGEX "\.dll\.a$")
    endif()
    list(TRANSFORM src_file_names PREPEND "${cargo_build_dir}/")
    list(TRANSFORM file_names PREPEND "${output_dir}/" OUTPUT_VARIABLE dst_file_names)
    message(DEBUG "Adding command to copy byproducts `${file_names}` to ${dst_file_names}")
    add_custom_command(TARGET _cargo-build_${target_name}
                        POST_BUILD
                        # output_dir may contain a Generator expression.
                        COMMAND  ${CMAKE_COMMAND} -E make_directory "${output_dir}"
                        COMMAND
                        ${CMAKE_COMMAND} -E copy_if_different
                            # tested to work with both multiple files and paths with spaces
                            ${src_file_names}
                            "${output_dir}"
                        BYPRODUCTS ${dst_file_names}
                        COMMENT "Copying byproducts `${file_names}` to ${output_dir}"
                        VERBATIM
                        COMMAND_EXPAND_LISTS
    )
endfunction()

# Copy the artifacts generated by cargo to the appropriate destination.
#
# Parameters:
# - target_name: The name of the Rust target
# - output_dir_prop_names: The property name(s) controlling the destination (e.g.
#   `LIBRARY_OUTPUT_DIRECTORY` or `PDB_OUTPUT_DIRECTORY;RUNTIME_OUTPUT_DIRECTORY`)
# - cargo_build_dir: the directory cargo build places it's output artifacts in.
# - filenames: the file names of any output artifacts as a list.
function(_corrosion_copy_byproducts target_name output_dir_prop_names cargo_build_dir file_names)
        cmake_language(EVAL CODE "
            cmake_language(DEFER
                CALL
                _corrosion_copy_byproduct_deferred
                [[${target_name}]]
                [[${output_dir_prop_names}]]
                [[${cargo_build_dir}]]
                [[${file_names}]]
            )
        ")
endfunction()


# Add targets for the static and/or shared libraries of the rust target.
# The generated byproduct names are returned via the `OUT_<type>_BYPRODUCTS` arguments.
function(_corrosion_add_library_target)
    set(OPTIONS "")
    set(ONE_VALUE_KEYWORDS
        WORKSPACE_MANIFEST_PATH
        TARGET_NAME
        OUT_ARCHIVE_OUTPUT_BYPRODUCTS
        OUT_SHARED_LIB_BYPRODUCTS
        OUT_PDB_BYPRODUCT
    )
    set(MULTI_VALUE_KEYWORDS LIB_KINDS)
    cmake_parse_arguments(PARSE_ARGV 0 CALT "${OPTIONS}" "${ONE_VALUE_KEYWORDS}" "${MULTI_VALUE_KEYWORDS}")

    if(DEFINED CALT_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Internal error - unexpected arguments: ${CALT_UNPARSED_ARGUMENTS}")
    elseif(DEFINED CALT_KEYWORDS_MISSING_VALUES)
        message(FATAL_ERROR "Internal error - the following keywords had no associated value(s):"
            "${CALT_KEYWORDS_MISSING_VALUES}")
    endif()
    list(TRANSFORM ONE_VALUE_KEYWORDS PREPEND CALT_ OUTPUT_VARIABLE required_arguments)
    foreach(required_argument ${required_arguments} )
        if(NOT DEFINED "${required_argument}")
            message(FATAL_ERROR "Internal error: Missing required argument ${required_argument}."
                "Complete argument list: ${ARGN}"
            )
        endif()
    endforeach()
    if("staticlib" IN_LIST CALT_LIB_KINDS)
        set(has_staticlib TRUE)
    endif()
    if("cdylib" IN_LIST CALT_LIB_KINDS)
        set(has_cdylib TRUE)
    endif()

    if(NOT (has_staticlib OR has_cdylib))
        message(FATAL_ERROR "Unknown library type(s): ${CALT_LIB_KINDS}")
    endif()
    set(workspace_manifest_path "${CALT_WORKSPACE_MANIFEST_PATH}")
    set(target_name "${CALT_TARGET_NAME}")

    set(is_windows "")
    set(is_windows_gnu "")
    set(is_windows_msvc "")
    set(is_macos "")
    set(is_ios "")
    if(Rust_CARGO_TARGET_OS STREQUAL "windows")
        set(is_windows TRUE)
        if(Rust_CARGO_TARGET_ENV STREQUAL "msvc")
            set(is_windows_msvc TRUE)
        elseif(Rust_CARGO_TARGET_ENV STREQUAL "gnu" OR Rust_CARGO_TARGET_ENV STREQUAL "gnullvm")
            set(is_windows_gnu TRUE)
        endif()
    elseif(Rust_CARGO_TARGET_OS STREQUAL "darwin")
        set(is_macos TRUE)
    elseif(Rust_CARGO_TARGET_OS STREQUAL "ios")
        set(is_ios true)
    endif()

    # target file names
    string(REPLACE "-" "_" lib_name "${target_name}")

    if(is_windows_msvc)
        set(static_lib_name "${lib_name}.lib")
    else()
        set(static_lib_name "lib${lib_name}.a")
    endif()

    if(is_windows)
        set(dynamic_lib_name "${lib_name}.dll")
    elseif(is_macos OR is_ios)
        set(dynamic_lib_name "lib${lib_name}.dylib")
    else()
        set(dynamic_lib_name "lib${lib_name}.so")
    endif()

    if(is_windows_msvc)
        set(implib_name "${lib_name}.dll.lib")
    elseif(is_windows_gnu)
        set(implib_name "lib${lib_name}.dll.a")
    elseif(is_windows)
        message(FATAL_ERROR "Unknown windows environment - Can't determine implib name")
    endif()


    set(pdb_name "${lib_name}.pdb")

    set(archive_output_byproducts "")
    if(has_staticlib)
        list(APPEND archive_output_byproducts ${static_lib_name})
    endif()

    if(has_cdylib)
        set("${CALT_OUT_SHARED_LIB_BYPRODUCTS}" "${dynamic_lib_name}" PARENT_SCOPE)
        if(is_windows)
            list(APPEND archive_output_byproducts ${implib_name})
        endif()
        if(is_windows_msvc)
            set("${CALT_OUT_PDB_BYPRODUCT}" "${pdb_name}" PARENT_SCOPE)
        endif()
    endif()
    set("${CALT_OUT_ARCHIVE_OUTPUT_BYPRODUCTS}" "${archive_output_byproducts}" PARENT_SCOPE)

    if(has_staticlib)
        add_library(${target_name}-static STATIC IMPORTED GLOBAL)
        add_dependencies(${target_name}-static cargo-build_${target_name})
        set_target_properties(${target_name}-static PROPERTIES COR_FILE_NAME ${static_lib_name})

        _corrosion_set_imported_location("${target_name}-static" "IMPORTED_LOCATION"
                "ARCHIVE_OUTPUT_DIRECTORY"
                "${static_lib_name}")

        # Todo: NO_STD target property?
        if(NOT COR_NO_STD)
            set_property(
                    TARGET ${target_name}-static
                    PROPERTY INTERFACE_LINK_LIBRARIES ${Rust_CARGO_TARGET_LINK_NATIVE_LIBS}
            )
            set_property(
                    TARGET ${target_name}-static
                    PROPERTY INTERFACE_LINK_OPTIONS ${Rust_CARGO_TARGET_LINK_OPTIONS}
            )
            if(is_macos)
                set_property(TARGET ${target_name}-static
                        PROPERTY INTERFACE_LINK_DIRECTORIES "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib"
                        )
            endif()
        endif()
    endif()

    if(has_cdylib)
        add_library(${target_name}-shared SHARED IMPORTED GLOBAL)
        add_dependencies(${target_name}-shared cargo-build_${target_name})
        set_target_properties(${target_name}-shared PROPERTIES COR_FILE_NAME ${dynamic_lib_name})

        # Todo: (Not new issue): What about IMPORTED_SONAME and IMPORTED_NO_SYSTEM?
        _corrosion_set_imported_location("${target_name}-shared" "IMPORTED_LOCATION"
                "LIBRARY_OUTPUT_DIRECTORY"
                "${dynamic_lib_name}"
        )
        # In the future we would probably prefer to let Rust set the soname for packages >= 1.0.
        # This is tracked in issue #333.
        set_target_properties(${target_name}-shared PROPERTIES IMPORTED_NO_SONAME TRUE)

        if(is_windows)
            _corrosion_set_imported_location("${target_name}-shared" "IMPORTED_IMPLIB"
                    "ARCHIVE_OUTPUT_DIRECTORY"
                    "${implib_name}"
            )
            set_target_properties(${target_name}-shared PROPERTIES COR_IMPLIB_FILE_NAME ${implib_name})
        endif()

        if(is_macos)
            set_property(TARGET ${target_name}-shared
                    PROPERTY INTERFACE_LINK_DIRECTORIES "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib"
                    )
        endif()
    endif()

    if(has_cdylib AND has_staticlib)
        if(BUILD_SHARED_LIBS)
            target_link_libraries(${target_name} INTERFACE ${target_name}-shared)
        else()
            target_link_libraries(${target_name} INTERFACE ${target_name}-static)
        endif()
    elseif(has_cdylib)
        target_link_libraries(${target_name} INTERFACE ${target_name}-shared)
    else()
        target_link_libraries(${target_name} INTERFACE ${target_name}-static)
    endif()
endfunction()

function(_corrosion_add_bin_target workspace_manifest_path bin_name out_bin_byproduct out_pdb_byproduct)
    if(NOT bin_name)
        message(FATAL_ERROR "No bin_name in _corrosion_add_bin_target for target ${target_name}")
    endif()

    string(REPLACE "-" "_" bin_name_underscore "${bin_name}")

    set(pdb_name "${bin_name_underscore}.pdb")

    if(Rust_CARGO_TARGET_ENV STREQUAL "msvc")
        set(${out_pdb_byproduct} "${pdb_name}" PARENT_SCOPE)
    endif()

    # Potential .exe suffix will be added later, also depending on possible hostbuild
    # target property
    set(bin_filename "${bin_name}")
    set(${out_bin_byproduct} "${bin_filename}" PARENT_SCOPE)
    add_dependencies(${bin_name} cargo-build_${bin_name})

    if(Rust_CARGO_TARGET_OS STREQUAL "darwin")
        set_property(TARGET ${bin_name}
                PROPERTY INTERFACE_LINK_DIRECTORIES "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib"
                )
    endif()

    _corrosion_set_imported_location("${bin_name}" "IMPORTED_LOCATION"
                        "RUNTIME_OUTPUT_DIRECTORY"
                        "${bin_filename}"
    )

endfunction()


include(CorrosionGenerator)

# Note: `cmake_language(GET_MESSAGE_LOG_LEVEL <output_variable>)` requires CMake 3.25,
# so we offer our own option to control verbosity of downstream commands (e.g. cargo build)
if (CORROSION_VERBOSE_OUTPUT)
    set(_CORROSION_VERBOSE_OUTPUT_FLAG --verbose CACHE INTERNAL "")
else()
    # We want to silence some less important commands by default.
    set(_CORROSION_QUIET_OUTPUT_FLAG --quiet CACHE INTERNAL "")
endif()

set(_CORROSION_CARGO_VERSION ${Rust_CARGO_VERSION} CACHE INTERNAL "cargo version used by corrosion")
set(_CORROSION_RUST_CARGO_TARGET ${Rust_CARGO_TARGET} CACHE INTERNAL "target triple used by corrosion")
set(_CORROSION_RUST_CARGO_HOST_TARGET ${Rust_CARGO_HOST_TARGET} CACHE INTERNAL "host triple used by corrosion")
set(_CORROSION_RUSTC "${RUSTC_EXECUTABLE}" CACHE INTERNAL  "Path to rustc used by corrosion")
set(_CORROSION_CARGO "${CARGO_EXECUTABLE}" CACHE INTERNAL "Path to cargo used by corrosion")

string(REPLACE "-" "_" _CORROSION_RUST_CARGO_TARGET_UNDERSCORE "${Rust_CARGO_TARGET}")
set(_CORROSION_RUST_CARGO_TARGET_UNDERSCORE "${_CORROSION_RUST_CARGO_TARGET_UNDERSCORE}" CACHE INTERNAL "lowercase target triple with underscores")
string(TOUPPER "${_CORROSION_RUST_CARGO_TARGET_UNDERSCORE}" _CORROSION_TARGET_TRIPLE_UPPER)
set(_CORROSION_RUST_CARGO_TARGET_UPPER
        "${_CORROSION_TARGET_TRIPLE_UPPER}"
        CACHE INTERNAL
        "target triple in uppercase with underscore"
)

# We previously specified some Custom properties as part of our public API, however the chosen names prevented us from
# supporting CMake versions before 3.19. In order to both support older CMake versions and not break existing code
# immediately, we are using a different property name depending on the CMake version. However users avoid using
# any of the properties directly, as they are no longer part of the public API and are to be considered deprecated.
# Instead use the corrosion_set_... functions as documented in the Readme.
set(_CORR_PROP_FEATURES CORROSION_FEATURES CACHE INTERNAL "")
set(_CORR_PROP_ALL_FEATURES CORROSION_ALL_FEATURES CACHE INTERNAL "")
set(_CORR_PROP_NO_DEFAULT_FEATURES CORROSION_NO_DEFAULT_FEATURES CACHE INTERNAL "")
set(_CORR_PROP_ENV_VARS CORROSION_ENVIRONMENT_VARIABLES CACHE INTERNAL "")
set(_CORR_PROP_HOST_BUILD CORROSION_USE_HOST_BUILD CACHE INTERNAL "")

# Add custom command to build one target in a package (crate)
#
# A target may be either a specific bin
function(_add_cargo_build out_cargo_build_out_dir)
    set(options NO_LINKER_OVERRIDE)
    set(one_value_args PACKAGE TARGET MANIFEST_PATH WORKSPACE_MANIFEST_PATH)
    set(multi_value_args BYPRODUCTS TARGET_KINDS)
    cmake_parse_arguments(
        ACB
        "${options}"
        "${one_value_args}"
        "${multi_value_args}"
        ${ARGN}
    )

    if(DEFINED ACB_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Internal error - unexpected arguments: "
            ${ACB_UNPARSED_ARGUMENTS})
    elseif(DEFINED ACB_KEYWORDS_MISSING_VALUES)
        message(FATAL_ERROR "Internal error - missing values for the following arguments: "
                ${ACB_KEYWORDS_MISSING_VALUES})
    endif()

    set(package_name "${ACB_PACKAGE}")
    set(target_name "${ACB_TARGET}")
    set(path_to_toml "${ACB_MANIFEST_PATH}")
    set(target_kinds "${ACB_TARGET_KINDS}")
    set(workspace_manifest_path "${ACB_WORKSPACE_MANIFEST_PATH}")
    set(build_byproducts "${ACB_BYPRODUCTS}")

    unset(cargo_rustc_crate_types)
    if(NOT target_kinds)
        message(FATAL_ERROR "TARGET_KINDS not specified")
    elseif("staticlib" IN_LIST target_kinds OR "cdylib" IN_LIST target_kinds)
        set(cargo_rustc_filter "--lib")
        if("${Rust_VERSION}" VERSION_GREATER_EQUAL "1.64")
            # https://doc.rust-lang.org/1.64.0/cargo/commands/cargo-rustc.html
            # `--crate-type` is documented since Rust 1.64 for `cargo rustc`.
            # We just unconditionally set it when available, to support overriding the crate type.
            # Due to https://github.com/rust-lang/cargo/issues/14498 we can't use one argument and pass a
            # comma seperated list. Instead we use multiple arguments.
            set(cargo_rustc_crate_types "${target_kinds}")
            list(TRANSFORM cargo_rustc_crate_types PREPEND "--crate-type=")
        endif()
    elseif("bin" IN_LIST target_kinds)
        set(cargo_rustc_filter "--bin=${target_name}")
    else()
        message(FATAL_ERROR "TARGET_KINDS contained unknown kind `${target_kind}`")
    endif()

    if (NOT IS_ABSOLUTE "${path_to_toml}")
        set(path_to_toml "${CMAKE_SOURCE_DIR}/${path_to_toml}")
    endif()
    get_filename_component(workspace_toml_dir ${path_to_toml} DIRECTORY )

    if (CMAKE_VS_PLATFORM_NAME)
        set(build_dir "${CMAKE_VS_PLATFORM_NAME}/$<CONFIG>")
    elseif(COR_IS_MULTI_CONFIG)
        set(build_dir "$<CONFIG>")
    else()
        unset(build_dir)
    endif()

    # If a CMake sysroot is specified, forward it to the linker rustc invokes, too. CMAKE_SYSROOT is documented
    # to be passed via --sysroot, so we assume that when it's set, the linker supports this option in that style.
    if(CMAKE_CROSSCOMPILING AND CMAKE_SYSROOT)
        set(corrosion_link_args "--sysroot=${CMAKE_SYSROOT}")
    endif()

    if(COR_ALL_FEATURES)
        set(all_features_arg --all-features)
    endif()
    if(COR_NO_DEFAULT_FEATURES)
        set(no_default_features_arg --no-default-features)
    endif()
    if(COR_NO_USES_TERMINAL)
        unset(cor_uses_terminal)
    else()
        set(cor_uses_terminal USES_TERMINAL)
    endif()

    set(global_rustflags_target_property "$<TARGET_GENEX_EVAL:${target_name},$<TARGET_PROPERTY:${target_name},INTERFACE_CORROSION_RUSTFLAGS>>")
    set(local_rustflags_target_property  "$<TARGET_GENEX_EVAL:${target_name},$<TARGET_PROPERTY:${target_name},INTERFACE_CORROSION_LOCAL_RUSTFLAGS>>")

    # todo: this probably should be TARGET_GENEX_EVAL
    set(features_target_property "$<GENEX_EVAL:$<TARGET_PROPERTY:${target_name},${_CORR_PROP_FEATURES}>>")
    set(features_genex "$<$<BOOL:${features_target_property}>:--features=$<JOIN:${features_target_property},$<COMMA>>>")

    # target property overrides corrosion_import_crate argument
    set(all_features_target_property "$<GENEX_EVAL:$<TARGET_PROPERTY:${target_name},${_CORR_PROP_ALL_FEATURES}>>")
    set(all_features_arg "$<$<BOOL:${all_features_target_property}>:--all-features>")

    set(no_default_features_target_property "$<GENEX_EVAL:$<TARGET_PROPERTY:${target_name},${_CORR_PROP_NO_DEFAULT_FEATURES}>>")
    set(no_default_features_arg "$<$<BOOL:${no_default_features_target_property}>:--no-default-features>")

    set(build_env_variable_genex "$<GENEX_EVAL:$<TARGET_PROPERTY:${target_name},${_CORR_PROP_ENV_VARS}>>")
    set(hostbuild_override "$<BOOL:$<TARGET_PROPERTY:${target_name},${_CORR_PROP_HOST_BUILD}>>")
    set(if_not_host_build_condition "$<NOT:${hostbuild_override}>")

    set(corrosion_link_args "$<${if_not_host_build_condition}:${corrosion_link_args}>")
    # We always set `--target`, so that cargo always places artifacts into a directory with the
    # target triple.
    set(cargo_target_option "--target=$<IF:${hostbuild_override},${_CORROSION_RUST_CARGO_HOST_TARGET},${_CORROSION_RUST_CARGO_TARGET}>")

    # The target may be a filepath to custom target json file. For host targets we assume that they are built-in targets.
    _corrosion_strip_target_triple("${_CORROSION_RUST_CARGO_TARGET}" stripped_target_triple)
    _corrosion_strip_target_triple("${_CORROSION_RUST_CARGO_TARGET_UPPER}" stripped_target_triple_upper)
    set(target_artifact_dir "$<IF:${hostbuild_override},${_CORROSION_RUST_CARGO_HOST_TARGET},${stripped_target_triple}>")

    set(flags_genex "$<GENEX_EVAL:$<TARGET_PROPERTY:${target_name},INTERFACE_CORROSION_CARGO_FLAGS>>")

    set(explicit_linker_property "$<TARGET_PROPERTY:${target_name},INTERFACE_CORROSION_LINKER>")
    set(explicit_linker_defined "$<BOOL:${explicit_linker_property}>")

    set(cargo_profile_target_property "$<TARGET_GENEX_EVAL:${target_name},$<TARGET_PROPERTY:${target_name},INTERFACE_CORROSION_CARGO_PROFILE>>")

    # Option to override the rustc/cargo binary to something other than the global default
    set(rustc_override "$<TARGET_PROPERTY:${target_name},INTERFACE_CORROSION_RUSTC>")
    set(cargo_override "$<TARGET_PROPERTY:${target_name},INTERFACE_CORROSION_CARGO>")
    set(rustc_bin "$<IF:$<BOOL:${rustc_override}>,${rustc_override},${_CORROSION_RUSTC}>")
    set(cargo_bin "$<IF:$<BOOL:${cargo_override}>,${cargo_override},${_CORROSION_CARGO}>")


    # Rust will add `-lSystem` as a flag for the linker on macOS. Adding the -L flag via RUSTFLAGS only fixes the
    # problem partially - buildscripts still break, since they won't receive the RUSTFLAGS. This seems to only be a
    # problem if we specify the linker ourselves (which we do, since this is necessary for e.g. linking C++ code).
    # We can however set `LIBRARY_PATH`, which is propagated to the build-script-build properly.
    if(NOT CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        # not needed anymore on macos 13 (and causes issues)
        if(${CMAKE_SYSTEM_VERSION} VERSION_LESS 22)
        set(cargo_library_path "LIBRARY_PATH=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib")
        endif()
    elseif(CMAKE_CROSSCOMPILING AND CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
        if(${CMAKE_HOST_SYSTEM_VERSION} VERSION_LESS 22)
            set(cargo_library_path "$<${hostbuild_override}:LIBRARY_PATH=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib>")
        endif()
    endif()

    set(cargo_profile_set "$<BOOL:${cargo_profile_target_property}>")
    # In the default case just specify --release or nothing to stay compatible with
    # older rust versions.
    set(default_profile_option "$<$<NOT:$<OR:$<CONFIG:Debug>,$<CONFIG:>>>:--release>")
    # evaluates to either `--profile=<custom_profile>`, `--release` or nothing (for debug).
    set(cargo_profile "$<IF:${cargo_profile_set},--profile=${cargo_profile_target_property},${default_profile_option}>")

    # If the profile name is `dev` change the dir name to `debug`.
    set(is_dev_profile "$<STREQUAL:${cargo_profile_target_property},dev>")
    set(profile_dir_override "$<${is_dev_profile}:debug>")
    set(profile_dir_is_overridden "$<BOOL:${profile_dir_override}>")
    set(custom_profile_build_type_dir "$<IF:${profile_dir_is_overridden},${profile_dir_override},${cargo_profile_target_property}>")

    set(default_build_type_dir "$<IF:$<OR:$<CONFIG:Debug>,$<CONFIG:>>,debug,release>")
    set(build_type_dir "$<IF:${cargo_profile_set},${custom_profile_build_type_dir},${default_build_type_dir}>")

    # We set a target folder based on the manifest path so if you build multiple workspaces (or standalone projects
    # without workspace) they won't collide if they use a common dependency. This would confuse cargo and trigger
    # unnecessary rebuilds
    cmake_path(GET workspace_manifest_path PARENT_PATH parent_path)
    cmake_path(GET parent_path PARENT_PATH grandparent_path)
    string(REPLACE "${grandparent_path}/" "" cargo_folder_name "${parent_path}")
    string(SHA1 cargo_path_hash ${workspace_manifest_path})
    # Include a hash of the full path in case there are multiple projects with the same folder name
    string(SUBSTRING "${cargo_path_hash}" 0 5 cargo_path_hash)
    cmake_path(APPEND CMAKE_BINARY_DIR ${build_dir} cargo "${cargo_folder_name}_${cargo_path_hash}"
               OUTPUT_VARIABLE cargo_target_dir)
    set(cargo_build_dir "${cargo_target_dir}/${target_artifact_dir}/${build_type_dir}")
    set("${out_cargo_build_out_dir}" "${cargo_build_dir}" PARENT_SCOPE)

    set(corrosion_cc_rs_flags)

    if(CMAKE_C_COMPILER)
        # This variable is read by cc-rs (often used in build scripts) to determine the c-compiler.
        # It can still be overridden if the user sets the non underscore variant via the environment variables
        # on the target.
        list(APPEND corrosion_cc_rs_flags "CC_${stripped_target_triple}=${CMAKE_C_COMPILER}")
    endif()
    if(CMAKE_CXX_COMPILER)
        list(APPEND corrosion_cc_rs_flags "CXX_${stripped_target_triple}=${CMAKE_CXX_COMPILER}")
    endif()
    # cc-rs doesn't seem to support `llvm-ar` (commandline syntax), wo we might as well just use
    # the default AR.
    if(CMAKE_AR AND NOT (Rust_CARGO_TARGET_ENV STREQUAL "msvc"))
        list(APPEND corrosion_cc_rs_flags "AR_${stripped_target_triple}=${CMAKE_AR}")
    endif()

    # When using XCode to target iOS / iOSSimulator, `cc` will be a compiler that targets iOS.
    # (Presumably this is because XCode modifies PATH).
    # This causes linker errors, because Rust compiles build-scripts and proc-macros for the host-platform, and
    # assumes `cc` is a valid linker driver for the host platform (but in this case `cc` targets iOS).
    # To work around this we explicitly set the linker for the host platform.
    unset(cargo_host_target_linker)
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_SYSTEM_NAME STREQUAL "iOS")
        string(TOUPPER "${Rust_CARGO_HOST_TARGET_CACHED}" host_target_upper)
        string(REPLACE "-" "_" host_target_upper_underscore "${host_target_upper}")
        set(cargo_host_target_linker "CARGO_TARGET_${host_target_upper_underscore}_LINKER=$CACHE{CORROSION_HOST_TARGET_LINKER}")
        message(DEBUG "Setting `${cargo_host_target_linker}` for target ${target_name} to workaround a hostbuild"
            " issue when building targets for iOS."
        )
    endif()

    # Since we instruct cc-rs to use the compiler found by CMake, it is likely one that requires also
    # specifying the target sysroot to use. CMake's generator makes sure to pass --sysroot with
    # CMAKE_OSX_SYSROOT. Fortunately the compilers Apple ships also respect the SDKROOT environment
    # variable, which we can set for use when cc-rs invokes the compiler.
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_OSX_SYSROOT)
        list(APPEND corrosion_cc_rs_flags "SDKROOT=${CMAKE_OSX_SYSROOT}")
    endif()

    # Ensure that cc-rs targets same Apple platform version as the CMake build
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_OSX_DEPLOYMENT_TARGET)
        list(APPEND corrosion_cc_rs_flags "MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    endif()

    corrosion_add_target_local_rustflags("${target_name}" "$<$<BOOL:${corrosion_link_args}>:-Clink-args=${corrosion_link_args}>")

    # todo: this should probably also be guarded by if_not_host_build_condition.
    if(COR_NO_STD)
        corrosion_add_target_local_rustflags("${target_name}" "-Cdefault-linker-libraries=no")
    else()
        corrosion_add_target_local_rustflags("${target_name}" "-Cdefault-linker-libraries=yes")
    endif()

    set(global_joined_rustflags "$<JOIN:${global_rustflags_target_property}, >")
    set(global_rustflags_genex "$<$<BOOL:${global_rustflags_target_property}>:RUSTFLAGS=${global_joined_rustflags}>")
    set(local_rustflags_delimiter "$<$<BOOL:${local_rustflags_target_property}>:-->")
    set(local_rustflags_genex "$<$<BOOL:${local_rustflags_target_property}>:${local_rustflags_target_property}>")

    set(deps_link_languages_prop "$<TARGET_PROPERTY:_cargo-build_${target_name},CARGO_DEPS_LINKER_LANGUAGES>")
    set(deps_link_languages "$<TARGET_GENEX_EVAL:_cargo-build_${target_name},${deps_link_languages_prop}>")
    set(target_uses_cxx  "$<IN_LIST:CXX,${deps_link_languages}>")
    unset(default_linker)
    # With the MSVC ABI rustc only supports directly invoking the linker - Invoking cl as the linker driver is not supported.
    if(NOT (Rust_CARGO_TARGET_ENV STREQUAL "msvc" OR COR_NO_LINKER_OVERRIDE))
        set(default_linker "$<IF:$<BOOL:${target_uses_cxx}>,${CMAKE_CXX_COMPILER},${CMAKE_C_COMPILER}>")
    endif()
    # Used to set a linker for a specific target-triple.
    set(cargo_target_linker_var "CARGO_TARGET_${stripped_target_triple_upper}_LINKER")
    set(linker "$<IF:${explicit_linker_defined},${explicit_linker_property},${default_linker}>")
    set(cargo_target_linker $<$<BOOL:${linker}>:${cargo_target_linker_var}=${linker}>)

    if(Rust_CROSSCOMPILING AND (CMAKE_C_COMPILER_TARGET OR CMAKE_CXX_COMPILER_TARGET))
        set(linker_target_triple "$<IF:$<BOOL:${target_uses_cxx}>,${CMAKE_CXX_COMPILER_TARGET},${CMAKE_C_COMPILER_TARGET}>")
        set(rustflag_linker_arg "-Clink-args=--target=${linker_target_triple}")
        set(rustflag_linker_arg "$<${if_not_host_build_condition}:${rustflag_linker_arg}>")
        # Skip adding the linker argument, if the linker is explicitly set, since the
        # explicit_linker_property will not be set when this function runs.
        # Passing this rustflag is necessary for clang.
        corrosion_add_target_local_rustflags("${target_name}" "$<$<NOT:${explicit_linker_defined}>:${rustflag_linker_arg}>")
    endif()

    message(DEBUG "TARGET ${target_name} produces byproducts ${build_byproducts}")
    message(DEBUG "corrosion_cc_rs_flags: ${corrosion_cc_rs_flags}")

    add_custom_target(
        _cargo-build_${target_name}
        # Build crate
        COMMAND
            ${CMAKE_COMMAND} -E env
                "${build_env_variable_genex}"
                "${global_rustflags_genex}"
                "${cargo_target_linker}"
                "${cargo_host_target_linker}"
                "${corrosion_cc_rs_flags}"
                "${cargo_library_path}"
                "CORROSION_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}"
                "CARGO_BUILD_RUSTC=${rustc_bin}"
            "${cargo_bin}"
                rustc
                ${cargo_rustc_filter}
                ${cargo_target_option}
                ${_CORROSION_VERBOSE_OUTPUT_FLAG}
                ${all_features_arg}
                ${no_default_features_arg}
                ${features_genex}
                --package ${package_name}
                ${cargo_rustc_crate_types}
                --manifest-path "${path_to_toml}"
                --target-dir "${cargo_target_dir}"
                ${cargo_profile}
                ${flags_genex}
                # Any arguments to cargo must be placed before this line
                ${local_rustflags_delimiter}
                ${local_rustflags_genex}

        # Note: `BYPRODUCTS` may not contain **target specific** generator expressions.
        # This means we cannot use `${cargo_build_dir}`, since it currently uses `$<TARGET_PROPERTY>`
        # to determine the correct target directory, depending on if the hostbuild target property is
        # set or not.
        # BYPRODUCTS  "${cargo_build_dir}/${build_byproducts}"
        
        # Set WORKING_DIRECTORY to the directory containing the manifest, so that configuration files
        # such as `.cargo/config.toml` or `toolchain.toml` are applied as expected. Cargo searches for
        # configuration files by walking upward from the current directory.
        WORKING_DIRECTORY "${workspace_toml_dir}"
        ${cor_uses_terminal}
        COMMAND_EXPAND_LISTS
        VERBATIM
    )

    # User exposed custom target, that depends on the internal target.
    # Corrosion post build steps are added on the internal target, which
    # ensures that they run before any user defined post build steps on this
    # target.
    add_custom_target(
        cargo-build_${target_name}
        ALL
    )
    add_dependencies(cargo-build_${target_name} _cargo-build_${target_name})

    # Add custom target before actual build that user defined custom commands (e.g. code generators) can
    # use as a hook to do something before the build. This mainly exists to not expose the `_cargo-build` targets.
    add_custom_target(cargo-prebuild_${target_name})
    add_dependencies(_cargo-build_${target_name} cargo-prebuild_${target_name})
    if(NOT TARGET cargo-prebuild)
        add_custom_target(cargo-prebuild)
    endif()
    add_dependencies(cargo-prebuild cargo-prebuild_${target_name})

    add_custom_target(
        cargo-clean_${target_name}
        COMMAND
            "${cargo_bin}" clean ${cargo_target_option}
            -p ${package_name} --manifest-path "${path_to_toml}"
        # Set WORKING_DIRECTORY to the directory containing the manifest, so that configuration files
        # such as `.cargo/config.toml` or `toolchain.toml` are applied as expected. Cargo searches for
        # configuration files by walking upward from the current directory.
        WORKING_DIRECTORY "${workspace_toml_dir}"
        ${cor_uses_terminal}
    )

    if (NOT TARGET cargo-clean)
        add_custom_target(cargo-clean)
    endif()
    add_dependencies(cargo-clean cargo-clean_${target_name})
endfunction()

#[=======================================================================[.md:
ANCHOR: corrosion-import-crate
```cmake
corrosion_import_crate(
        MANIFEST_PATH <path/to/cargo.toml>
        [ALL_FEATURES]
        [NO_DEFAULT_FEATURES]
        [NO_STD]
        [NO_LINKER_OVERRIDE]
        [NO_USES_TERMINAL]
        [LOCKED]
        [FROZEN]
        [PROFILE <cargo-profile>]
        [IMPORTED_CRATES <variable-name>]
        [CRATE_TYPES <crate_type1> ... <crate_typeN>]
        [OVERRIDE_CRATE_TYPE <crate_name>=<crate_type1,crate_type2,...> ...]
        [CRATES <crate1> ... <crateN>]
        [FEATURES <feature1> ... <featureN>]
        [FLAGS <flag1> ... <flagN>]
)
```
* **MANIFEST_PATH**: Path to a [Cargo.toml Manifest] file.
* **ALL_FEATURES**: Equivalent to [--all-features] passed to cargo build
* **NO_DEFAULT_FEATURES**: Equivalent to [--no-default-features] passed to cargo build
* **NO_STD**:  Disable linking of standard libraries (required for no_std crates).
* **NO_LINKER_OVERRIDE**: Will let Rust/Cargo determine which linker to use instead of corrosion (when linking is invoked by Rust)
* **NO_USES_TERMINAL**: Don't pass the `USES_TERMINAL` flag when creating the custom CMake targets.
* **LOCKED**: Pass [`--locked`] to cargo build and cargo metadata.
* **FROZEN**: Pass [`--frozen`] to cargo build and cargo metadata.
* **PROFILE**: Specify cargo build profile (`dev`/`release` or a [custom profile]; `bench` and `test` are not supported)
* **IMPORTED_CRATES**: Save the list of imported crates into the variable with the provided name in the current scope.
* **CRATE_TYPES**: Only import the specified crate types. Valid values: `staticlib`, `cdylib`, `bin`.
* **OVERRIDE_CRATE_TYPE**: Override the crate-types of a cargo crate with the given comma-separated values.
                           Internally uses the `rustc` flag [`--crate-type`] to override the crate-type.
                           Valid values for the crate types are the library types `staticlib` and `cdylib`.
* **CRATES**: Only import the specified crates from a workspace. Values: Crate names.
* **FEATURES**: Enable the specified features. Equivalent to [--features] passed to `cargo build`.
* **FLAGS**:  Arbitrary flags to `cargo build`.

[custom profile]: https://doc.rust-lang.org/cargo/reference/profiles.html#custom-profiles
[--all-features]: https://doc.rust-lang.org/cargo/reference/features.html#command-line-feature-options
[--no-default-features]: https://doc.rust-lang.org/cargo/reference/features.html#command-line-feature-options
[--features]: https://doc.rust-lang.org/cargo/reference/features.html#command-line-feature-options
[`--locked`]: https://doc.rust-lang.org/cargo/commands/cargo.html#manifest-options
[`--frozen`]: https://doc.rust-lang.org/cargo/commands/cargo.html#manifest-options
[`--crate-type`]: https://doc.rust-lang.org/rustc/command-line-arguments.html#--crate-type-a-list-of-types-of-crates-for-the-compiler-to-emit
[Cargo.toml Manifest]: https://doc.rust-lang.org/cargo/appendix/glossary.html#manifest

ANCHOR_END: corrosion-import-crate
#]=======================================================================]
function(corrosion_import_crate)
    set(OPTIONS
        ALL_FEATURES
        NO_DEFAULT_FEATURES
        NO_STD
        NO_LINKER_OVERRIDE
        NO_USES_TERMINAL
        LOCKED
        FROZEN)
    set(ONE_VALUE_KEYWORDS MANIFEST_PATH PROFILE IMPORTED_CRATES)
    set(MULTI_VALUE_KEYWORDS CRATE_TYPES CRATES FEATURES FLAGS OVERRIDE_CRATE_TYPE)
    cmake_parse_arguments(COR "${OPTIONS}" "${ONE_VALUE_KEYWORDS}" "${MULTI_VALUE_KEYWORDS}" ${ARGN})
    list(APPEND CMAKE_MESSAGE_CONTEXT "corrosion_import_crate")

    if(DEFINED COR_UNPARSED_ARGUMENTS)
        message(AUTHOR_WARNING "Unexpected arguments: " ${COR_UNPARSED_ARGUMENTS}
            "\nCorrosion will ignore these unexpected arguments."
            )
    endif()
    if(DEFINED COR_KEYWORDS_MISSING_VALUES)
        message(DEBUG "Note: the following keywords passed to corrosion_import_crate had no associated value(s): "
            ${COR_KEYWORDS_MISSING_VALUES}
        )
    endif()
    if (NOT DEFINED COR_MANIFEST_PATH)
        message(FATAL_ERROR "MANIFEST_PATH is a required keyword to corrosion_add_crate")
    endif()
    _corrosion_option_passthrough_helper(NO_LINKER_OVERRIDE COR no_linker_override)
    _corrosion_option_passthrough_helper(LOCKED COR locked)
    _corrosion_option_passthrough_helper(FROZEN COR frozen)
    _corrosion_arg_passthrough_helper(CRATES COR crate_allowlist)
    _corrosion_arg_passthrough_helper(CRATE_TYPES COR crate_types)

    if(COR_PROFILE)
        if(Rust_VERSION VERSION_LESS 1.57.0)
            message(FATAL_ERROR "Selecting custom profiles via `PROFILE` requires at least rust 1.57.0, but you "
                        "have ${Rust_VERSION}."
        )
        # The profile name could be part of a Generator expression, so this won't catch all occurences.
        # Since it is hard to add an error message for genex, we don't do that here.
        elseif("${COR_PROFILE}" STREQUAL "test" OR "${COR_PROFILE}" STREQUAL "bench")
            message(FATAL_ERROR "Corrosion does not support building Rust crates with the cargo profiles"
                    " `test` or `bench`. These profiles add a hash to the output artifact name that we"
                    " cannot predict. Please consider using a custom cargo profile which inherits from the"
                    " built-in profile instead."
            )
        endif()
    endif()

    # intended to be used with foreach(... ZIP_LISTS ...), meaning
    # that the crate_types at index i of `override_crate_type_types_list` are
    # for the package_name at index i of `override_crate_type_package_name_list`.
    # It would really be nice if CMake had structs or dicts.
    unset(override_crate_type_package_name_list)
    unset(override_crate_type_types_list)
    unset(OVERRIDE_CRATE_TYPE_ARGS)
    if(DEFINED COR_OVERRIDE_CRATE_TYPE)
        string(JOIN " " usage_help
               "Each argument to OVERRIDE_CRATE_TYPE must be of the form `<package_name>=<crate_type(s)>."
               "The package_name must be a valid cargo package name and the crate_type must be "
               "a comma-seperated list with valid values being `staticlib`, `cdylib` and `bin`"
        )
        foreach(entry IN LISTS COR_OVERRIDE_CRATE_TYPE)
            string(REPLACE "=" ";" key_val_list ${entry})
            list(LENGTH key_val_list key_val_list_len)
            if(NOT key_val_list_len EQUAL "2")
                message(FATAL_ERROR "Invalid argument: `${entry}` for parameter OVERRIDE_CRATE_TYPE!\n"
                    "${usage_help}"
                )
            endif()
            list(GET key_val_list "0" package_name)
            list(GET key_val_list "1" crate_types)
            list(APPEND override_crate_type_package_name_list "${package_name}")
            list(APPEND override_crate_type_types_list "${crate_types}")
        endforeach()
        list(LENGTH override_crate_type_package_name_list num_override_packages)
        list(LENGTH override_crate_type_types_list num_override_packages2)
        if("${Rust_VERSION}" VERSION_LESS "1.64")
            message(WARNING "OVERRIDE_CRATE_TYPE requires at Rust 1.64 or newer. Ignoring the option")
        elseif(NOT num_override_packages EQUAL num_override_packages2)
            message(WARNING "Internal error while parsing OVERRIDE_CRATE_TYPE arguments.\n"
                    "Corrosion will ignore this argument and continue."
            )
        else()
            # Pass by ref: we intentionally pass the list names here!
            set(override_crate_types_arg "OVERRIDE_CRATE_TYPE_ARGS" "override_crate_type_package_name_list" "override_crate_type_types_list")
        endif()
    endif()

    if (NOT IS_ABSOLUTE "${COR_MANIFEST_PATH}")
        set(COR_MANIFEST_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${COR_MANIFEST_PATH})
    endif()

    set(additional_cargo_flags ${COR_FLAGS})

    if(COR_LOCKED AND NOT "--locked" IN_LIST additional_cargo_flags)
        list(APPEND additional_cargo_flags  "--locked")
    endif()
    if(COR_FROZEN AND NOT "--frozen" IN_LIST additional_cargo_flags)
        list(APPEND additional_cargo_flags  "--frozen")
    endif()

    set(imported_crates "")

    _generator_add_cargo_targets(
        MANIFEST_PATH
            "${COR_MANIFEST_PATH}"
        IMPORTED_CRATES
            imported_crates
        ${crate_allowlist}
        ${crate_types}
        ${no_linker_override}
        ${override_crate_types_arg}
    )

    # Not target props yet:
    # NO_STD
    # NO_LINKER_OVERRIDE # We could simply zero INTERFACE_CORROSION_LINKER if this is set.
    # LOCKED / FROZEN get merged into FLAGS after cargo metadata.

    # Initialize the target properties with the arguments to corrosion_import_crate.
    set_target_properties(
            ${imported_crates}
            PROPERTIES
                "${_CORR_PROP_ALL_FEATURES}" "${COR_ALL_FEATURES}"
                "${_CORR_PROP_NO_DEFAULT_FEATURES}" "${COR_NO_DEFAULT_FEATURES}"
                "${_CORR_PROP_FEATURES}" "${COR_FEATURES}"
                INTERFACE_CORROSION_CARGO_PROFILE "${COR_PROFILE}"
                INTERFACE_CORROSION_CARGO_FLAGS "${additional_cargo_flags}"
    )

    # _CORR_PROP_ENV_VARS
    if(DEFINED COR_IMPORTED_CRATES)
        set(${COR_IMPORTED_CRATES} ${imported_crates} PARENT_SCOPE)
    endif()
endfunction()

function(corrosion_set_linker target_name linker)
    if(NOT linker)
        message(FATAL_ERROR "The linker passed to `corrosion_set_linker` may not be empty")
    elseif(NOT TARGET "${target_name}")
        message(FATAL_ERROR "The target `${target_name}` does not exist.")
    endif()
    if(MSVC)
        message(WARNING "Explicitly setting the linker with the MSVC toolchain is currently not supported and ignored")
    endif()

    if(TARGET "${target_name}-static" AND NOT TARGET "${target_name}-shared")
        message(WARNING "The target ${target_name} builds a static library."
            "The linker is never invoked for a static library so specifying a linker has no effect."
        )
    endif()

    set_property(
        TARGET ${target_name}
        PROPERTY INTERFACE_CORROSION_LINKER "${linker}"
    )
endfunction()

function(corrosion_set_hostbuild target_name)
    # Configure the target to be compiled for the Host target and ignore any cross-compile configuration.
    set_property(
            TARGET ${target_name}
            PROPERTY ${_CORR_PROP_HOST_BUILD} 1
    )
endfunction()

# Add flags for rustc (RUSTFLAGS) which affect the target and all of it's Rust dependencies
#
# Additional rustflags may be passed as optional parameters after rustflag.
# Please note, that if you import multiple targets from a package or workspace, but set different
# Rustflags via this function, the Rust dependencies will have to be rebuilt when changing targets.
# Consider `corrosion_add_target_local_rustflags()` as an alternative to avoid this.
function(corrosion_add_target_rustflags target_name rustflag)
    # Additional rustflags may be passed as optional parameters after rustflag.
    set_property(
            TARGET ${target_name}
            APPEND
            PROPERTY INTERFACE_CORROSION_RUSTFLAGS ${rustflag} ${ARGN}
    )
endfunction()

# Add flags for rustc (RUSTFLAGS) which only affect the target, but none of it's (Rust) dependencies
#
# Additional rustflags may be passed as optional parameters after rustc_flag.
function(corrosion_add_target_local_rustflags target_name rustc_flag)
    # Set Rustflags via `cargo rustc` which only affect the current crate, but not dependencies.
    set_property(
            TARGET ${target_name}
            APPEND
            PROPERTY INTERFACE_CORROSION_LOCAL_RUSTFLAGS ${rustc_flag} ${ARGN}
    )
endfunction()

function(corrosion_set_env_vars target_name env_var)
    # Additional environment variables may be passed as optional parameters after env_var.
    set_property(
        TARGET ${target_name}
        APPEND
        PROPERTY ${_CORR_PROP_ENV_VARS} ${env_var} ${ARGN}
    )
endfunction()

function(corrosion_set_cargo_flags target_name)
    # corrosion_set_cargo_flags(<target_name> [<flag1> ... ])

    set_property(
            TARGET ${target_name}
            APPEND
            PROPERTY INTERFACE_CORROSION_CARGO_FLAGS ${ARGN}
    )
endfunction()

function(corrosion_set_features target_name)
    # corrosion_set_features(<target_name> [ALL_FEATURES=Bool] [NO_DEFAULT_FEATURES] [FEATURES <feature1> ... ])
    set(options NO_DEFAULT_FEATURES)
    set(one_value_args ALL_FEATURES)
    set(multi_value_args FEATURES)
    cmake_parse_arguments(
            PARSE_ARGV 1
            SET
            "${options}"
            "${one_value_args}"
            "${multi_value_args}"
    )

    if(DEFINED SET_ALL_FEATURES)
        set_property(
                TARGET ${target_name}
                PROPERTY ${_CORR_PROP_ALL_FEATURES} ${SET_ALL_FEATURES}
        )
    endif()
    if(SET_NO_DEFAULT_FEATURES)
        set_property(
                TARGET ${target_name}
                PROPERTY ${_CORR_PROP_NO_DEFAULT_FEATURES} 1
        )
    endif()
    if(SET_FEATURES)
        set_property(
                TARGET ${target_name}
                APPEND
                PROPERTY ${_CORR_PROP_FEATURES} ${SET_FEATURES}
        )
    endif()
endfunction()

function(corrosion_link_libraries target_name)
    if(TARGET "${target_name}-static")
        message(DEBUG "The target ${target_name} builds a static Rust library."
                "Calling `target_link_libraries()` instead."
        )
        target_link_libraries("${target_name}-static" INTERFACE ${ARGN})
        if(NOT TARGET "${target_name}-shared")
            # Early return, since Rust won't invoke the linker for static libraries
            return()
        endif()
    endif()
    foreach(library ${ARGN})
        set_property(
            TARGET _cargo-build_${target_name}
            APPEND
            PROPERTY CARGO_DEPS_LINKER_LANGUAGES
            $<TARGET_PROPERTY:${library},LINKER_LANGUAGE>
        )

        if (TARGET "${library}")
            # This works fine, except when compiling for ios. See https://cmake.org/pipermail/cmake/2016-March/063050.html
            # XCODE_EMIT_EFFECTIVE_PLATFORM_NAME=OFF is supposed to prevent emitting EFFECTIVE_PLATFORM_NAME, but even
            # with CMake 4.1 and the variable set to off EFFECTIVE_PLATFORM_NAME still leaks into generator expressions,
            # and is not correctly replaced at build time
            set(linker_dir "$<TARGET_LINKER_FILE_DIR:${library}>")
            # Probably should also affect other apple OSs with a simulator
            if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
                unset(platform_name)
                message(CHECK_START "corrosion_link_libraries: Attempting to replace EFFECTIVE_PLATFORM_NAME")
                if(CMAKE_OSX_SYSROOT MATCHES "iphoneos")
                    set(platform_name "-iphoneos")
                elseif(CMAKE_OSX_SYSROOT MATCHES "iphonesimulator")
                    set(platform_name "-iphonesimulator")
                else()
                    # Todo: CMAKE_OSX_SYSROOT can be not set - how do we handle that?
                    message(CHECK_FAIL "Failed to determine platform name for iOS target from sysroot ${CMAKE_OSX_SYSROOT}")
                endif()
                if(DEFINED platform_name)
                    # This is a hack to fix $EFFECTIVE_PLATFORM_NAME not expanding in TARGET_LINKER_FILE_DIR
                    set(linker_dir "$<PATH:REPLACE_FILENAME,${linker_dir},$<CONFIG>${platform_name}>")
                    message(CHECK_PASS "done")
                endif()
            endif()
            corrosion_add_target_local_rustflags(${target_name}
                "-L${linker_dir}"
                "-l$<TARGET_LINKER_FILE_BASE_NAME:${library}>"
            )
            add_dependencies(_cargo-build_${target_name} ${library})
        elseif(IS_ABSOLUTE "${library}")
            # Linking via full path (See https://doc.rust-lang.org/rustc/command-line-arguments.html#linking-modifiers-verbatim)
            corrosion_add_target_local_rustflags(${target_name} "-Clink-arg=${library}")
        else()
            # We have to assume ${library} is a non-CMake library name
            corrosion_add_target_local_rustflags(${target_name} "-l${library}")
        endif()
    endforeach()
endfunction()

#[=======================================================================[.md:
ANCHOR: corrosion-install
** EXPERIMENTAL **: This function is currently still considered experimental
  and is not officially released yet. Feedback and Suggestions are welcome.

```cmake
corrosion_install(TARGETS <target1> ... <targetN> [EXPORT <export-name>]
                  [[ARCHIVE|LIBRARY|RUNTIME|PUBLIC_HEADER]
                   [DESTINATION <dir>]
                   [PERMISSIONS <permissions...>]
                   [CONFIGURATIONS [Debug|Release|<other-configuration>]]
                  ] [...])
```
* **TARGETS**: Target or targets to install.
* **EXPORT**: Creates an export that can be installed with `install(EXPORT)`. <export-name> must be globally unique.
             Also creates a file at ${CMAKE_BINARY_DIR}/corrosion/<export-name>Corrosion.cmake that must be included in the installed config file.
* **ARCHIVE**/**LIBRARY**/**RUNTIME**/PUBLIC_HEADER: Designates that the following settings only apply to that specific type of object.
* **DESTINATION**: The subdirectory within the CMAKE_INSTALL_PREFIX that a specific object should be placed. Defaults to values from GNUInstallDirs.
* **PERMISSIONS**: The permissions of files copied into the install prefix.

Any `PUBLIC` or `INTERFACE` [file sets] will be installed.

[file sets]: https://cmake.org/cmake/help/latest/command/target_sources.html#file-sets

ANCHOR_END: corrosion-install
#]=======================================================================]
function(corrosion_install)
    # Default install dirs
    include(GNUInstallDirs)

    # Parse arguments to corrosion_install
    list(GET ARGN 0 INSTALL_TYPE)
    list(REMOVE_AT ARGN 0)

    # The different install types that are supported. Some targets may have more than one of these
    # types. For example, on Windows, a shared library will have both an ARCHIVE component and a
    # RUNTIME component.
    set(INSTALL_TARGET_TYPES ARCHIVE LIBRARY RUNTIME PRIVATE_HEADER PUBLIC_HEADER)

    # Arguments to each install target type
    set(OPTIONS)
    set(ONE_VALUE_ARGS DESTINATION)
    set(MULTI_VALUE_ARGS PERMISSIONS CONFIGURATIONS)
    set(TARGET_ARGS ${OPTIONS} ${ONE_VALUE_ARGS} ${MULTI_VALUE_ARGS})

    if (INSTALL_TYPE STREQUAL "TARGETS")
        # Extract targets
        set(INSTALL_TARGETS)
        list(LENGTH ARGN ARGN_LENGTH)
        set(DELIMITERS EXPORT ${INSTALL_TARGET_TYPES} ${TARGET_ARGS})
        while(ARGN_LENGTH)
            # If we hit another keyword, stop - we've found all the targets
            list(GET ARGN 0 FRONT)
            if (FRONT IN_LIST DELIMITERS)
                break()
            endif()

            list(APPEND INSTALL_TARGETS ${FRONT})
            list(REMOVE_AT ARGN 0)

            # Update ARGN_LENGTH
            list(LENGTH ARGN ARGN_LENGTH)
        endwhile()

        # Check if there are any args left before proceeding
        list(LENGTH ARGN ARGN_LENGTH)
        if (ARGN_LENGTH)
            list(GET ARGN 0 FRONT)
            if (FRONT STREQUAL "EXPORT")
                list(REMOVE_AT ARGN 0) # Pop "EXPORT"

                list(GET ARGN 0 EXPORT_NAME)
                list(REMOVE_AT ARGN 0) # Pop <export-name>
                set(EXTRA_TARGETS_EXPORT_NAME ${EXPORT_NAME}Corrosion.cmake)
                set(EXPORT_NAME EXPORT ${EXPORT_NAME})
                set(EXPORT_FILE_PATH "${CMAKE_BINARY_DIR}/corrosion/${EXTRA_TARGETS_EXPORT_NAME}")
                # Remove first, since otherwise we will append to the file on every reconfigure.
                # Assumes that the corrosion_install will only be called once for a given EXPORT_NAME.
                file(REMOVE "${EXPORT_FILE_PATH}")
            endif()
        else()
            # Prevent variable set in user code from interfering
            set(EXPORT_NAME)
        endif()

        # Loop over all arguments and get options for each install target type
        list(LENGTH ARGN ARGN_LENGTH)
        while(ARGN_LENGTH)
            # Check if we're dealing with arguments for a specific install target type, or with
            # default options for all target types.
            list(GET ARGN 0 FRONT)
            if (FRONT IN_LIST INSTALL_TARGET_TYPES)
                set(INSTALL_TARGET_TYPE ${FRONT})
                list(REMOVE_AT ARGN 0)
            else()
                set(INSTALL_TARGET_TYPE DEFAULT)
            endif()

            # Gather the arguments to this install type
            set(ARGS)
            list(LENGTH ARGN ARGN_LENGTH)
            while(ARGN_LENGTH)
                # If the next keyword is an install target type, then break - arguments have been
                # gathered.
                list(GET ARGN 0 FRONT)
                if (FRONT IN_LIST INSTALL_TARGET_TYPES)
                    break()
                endif()

                list(APPEND ARGS ${FRONT})
                list(REMOVE_AT ARGN 0)

                list(LENGTH ARGN ARGN_LENGTH)
            endwhile()

            # Parse the arguments and register the file install
            cmake_parse_arguments(
                COR "${OPTIONS}" "${ONE_VALUE_ARGS}" "${MULTI_VALUE_ARGS}" ${ARGS})

            if (COR_DESTINATION)
                set(COR_INSTALL_${INSTALL_TARGET_TYPE}_DESTINATION ${COR_DESTINATION})
            endif()

            if (COR_PERMISSIONS)
                set(COR_INSTALL_${INSTALL_TARGET_TYPE}_PERMISSIONS ${COR_PERMISSIONS})
            endif()

            if (COR_CONFIGURATIONS)
                set(COR_INSTALL_${INSTALL_TARGET_TYPE}_CONFIGURATIONS ${COR_CONFIGURATIONS})
            endif()

            # Update ARG_LENGTH
            list(LENGTH ARGN ARGN_LENGTH)
        endwhile()

        # Default permissions for all files
        set(DEFAULT_PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ)

        # Loop through each install target and register file installations
        foreach(INSTALL_TARGET ${INSTALL_TARGETS})
            if(NOT TARGET ${INSTALL_TARGET})
                message(FATAL_ERROR "Install target ${INSTALL_TARGET} is not a valid target")
            endif()
            # Don't both implementing target type differentiation using generator expressions since
            # TYPE cannot change after target creation
            get_property(
                TARGET_TYPE
                TARGET ${INSTALL_TARGET} PROPERTY TYPE
            )

            # Install executable files first
            if (TARGET_TYPE STREQUAL "EXECUTABLE")
                if (DEFINED COR_INSTALL_RUNTIME_DESTINATION)
                    set(DESTINATION ${COR_INSTALL_RUNTIME_DESTINATION})
                elseif (DEFINED COR_INSTALL_DEFAULT_DESTINATION)
                    set(DESTINATION ${COR_INSTALL_DEFAULT_DESTINATION})
                else()
                    set(DESTINATION ${CMAKE_INSTALL_BINDIR})
                endif()

                if (DEFINED COR_INSTALL_RUNTIME_PERMISSIONS)
                    set(PERMISSIONS ${COR_INSTALL_RUNTIME_PERMISSIONS})
                elseif (DEFINED COR_INSTALL_DEFAULT_PERMISSIONS)
                    set(PERMISSIONS ${COR_INSTALL_DEFAULT_PERMISSIONS})
                else()
                    set(
                        PERMISSIONS
                        ${DEFAULT_PERMISSIONS} OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE)
                endif()

                if (DEFINED COR_INSTALL_RUNTIME_CONFIGURATIONS)
                    set(CONFIGURATIONS CONFIGURATIONS ${COR_INSTALL_RUNTIME_CONFIGURATIONS})
                elseif (DEFINED COR_INSTALL_DEFAULT_CONFIGURATIONS)
                    set(CONFIGURATIONS CONFIGURATIONS ${COR_INSTALL_DEFAULT_CONFIGURATIONS})
                else()
                    set(CONFIGURATIONS)
                endif()

                install(
                    FILES $<TARGET_FILE:${INSTALL_TARGET}>
                    DESTINATION ${DESTINATION}
                    PERMISSIONS ${PERMISSIONS}
                    ${CONFIGURATIONS}
                )
            elseif(TARGET_TYPE STREQUAL "INTERFACE_LIBRARY")
                if(TARGET ${INSTALL_TARGET}-static)
                    if (DEFINED COR_INSTALL_ARCHIVE_DESTINATION)
                        set(DESTINATION ${COR_INSTALL_ARCHIVE_DESTINATION})
                    elseif (DEFINED COR_INSTALL_DEFAULT_DESTINATION)
                        set(DESTINATION ${COR_INSTALL_DEFAULT_DESTINATION})
                    else()
                        set(DESTINATION ${CMAKE_INSTALL_LIBDIR})
                    endif()

                    if (DEFINED COR_INSTALL_ARCHIVE_PERMISSIONS)
                        set(PERMISSIONS ${COR_INSTALL_ARCHIVE_PERMISSIONS})
                    elseif (DEFINED COR_INSTALL_DEFAULT_PERMISSIONS)
                        set(PERMISSIONS ${COR_INSTALL_DEFAULT_PERMISSIONS})
                    else()
                        set(PERMISSIONS ${DEFAULT_PERMISSIONS})
                    endif()

                    if (DEFINED COR_INSTALL_ARCHIVE_CONFIGURATIONS)
                        set(CONFIGURATIONS CONFIGURATIONS ${COR_INSTALL_ARCHIVE_CONFIGURATIONS})
                    elseif (DEFINED COR_INSTALL_DEFAULT_CONFIGURATIONS)
                        set(CONFIGURATIONS CONFIGURATIONS ${COR_INSTALL_DEFAULT_CONFIGURATIONS})
                    else()
                        set(CONFIGURATIONS)
                    endif()

                    install(
                            FILES $<TARGET_PROPERTY:${INSTALL_TARGET}-static,IMPORTED_LOCATION>
                            DESTINATION ${DESTINATION}
                            PERMISSIONS ${PERMISSIONS}
                            ${CONFIGURATIONS}
                    )

                    if(EXPORT_NAME)
                        get_target_property(COR_FILE_NAME ${INSTALL_TARGET}-static COR_FILE_NAME)
                        file(APPEND "${EXPORT_FILE_PATH}"
"
add_library(${INSTALL_TARGET}-static STATIC IMPORTED)
set_target_properties(${INSTALL_TARGET}-static
    PROPERTIES
    IMPORTED_LOCATION \"\${PACKAGE_PREFIX_DIR}/${DESTINATION}/${COR_FILE_NAME}\"
)
"
                        )
                    endif()
                endif()

                if(TARGET ${INSTALL_TARGET}-shared)
                    if (DEFINED COR_INSTALL_LIBRARY_DESTINATION)
                        set(DESTINATION ${COR_INSTALL_LIBRARY_DESTINATION})
                    elseif (DEFINED COR_INSTALL_DEFAULT_DESTINATION)
                        set(DESTINATION ${COR_INSTALL_DEFAULT_DESTINATION})
                    else()
                        set(DESTINATION ${CMAKE_INSTALL_LIBDIR})
                    endif()

                    if (DEFINED COR_INSTALL_LIBRARY_PERMISSIONS)
                        set(PERMISSIONS ${COR_INSTALL_LIBRARY_PERMISSIONS})
                    elseif (DEFINED COR_INSTALL_DEFAULT_PERMISSIONS)
                        set(PERMISSIONS ${COR_INSTALL_DEFAULT_PERMISSIONS})
                    else()
                        set(
                            PERMISSIONS
                            ${DEFAULT_PERMISSIONS} OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE
                        )
                    endif()

                    if (DEFINED COR_INSTALL_LIBRARY_CONFIGURATIONS)
                        set(CONFIGURATIONS CONFIGURATIONS ${COR_INSTALL_LIBRARY_CONFIGURATIONS})
                    elseif (DEFINED COR_INSTALL_DEFAULT_CONFIGURATIONS)
                        set(CONFIGURATIONS CONFIGURATIONS ${COR_INSTALL_DEFAULT_CONFIGURATIONS})
                    else()
                        set(CONFIGURATIONS)
                    endif()

                    install(
                            IMPORTED_RUNTIME_ARTIFACTS ${INSTALL_TARGET}-shared
                            PERMISSIONS ${PERMISSIONS}
                            DESTINATION ${DESTINATION}
                            ${CONFIGURATIONS}
                    )

                    if(EXPORT_NAME)
                        get_target_property(COR_FILE_NAME ${INSTALL_TARGET}-shared COR_FILE_NAME)
                        file(APPEND "${EXPORT_FILE_PATH}"
"
add_library(${INSTALL_TARGET}-shared SHARED IMPORTED)
set_target_properties(${INSTALL_TARGET}-shared
    PROPERTIES
    IMPORTED_LOCATION \"\${PACKAGE_PREFIX_DIR}/${DESTINATION}/${COR_FILE_NAME}\"
)
"
                            )

                            get_target_property(COR_IMPLIB_FILE_NAME ${INSTALL_TARGET}-shared COR_IMPLIB_FILE_NAME)
                            if (NOT COR_IMPLIB_FILE_NAME MATCHES .*-NOTFOUND)
                                file(APPEND "${EXPORT_FILE_PATH}"
"
set_target_properties(${INSTALL_TARGET}-shared
    PROPERTIES
    IMPORTED_IMPLIB \"\${PACKAGE_PREFIX_DIR}/${DESTINATION}/${COR_IMPLIB_FILE_NAME}\"
)"
                                )
                            endif()
                    endif()
                endif()
            else()
                message(FATAL_ERROR "Unknown target type ${TARGET_TYPE} for install target ${INSTALL_TARGET}")
            endif()

            # Executables can also have export tables, so they _might_ also need header files
            if (DEFINED COR_INSTALL_PUBLIC_HEADER_DESTINATION)
                set(DESTINATION ${COR_INSTALL_PUBLIC_HEADER_DESTINATION})
            elseif (DEFINED COR_INSTALL_DEFAULT_DESTINATION)
                set(DESTINATION ${COR_INSTALL_DEFAULT_DESTINATION})
            else()
                set(DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
            endif()

            if (DEFINED COR_INSTALL_PUBLIC_HEADER_PERMISSIONS)
                set(PERMISSIONS ${COR_INSTALL_PUBLIC_HEADER_PERMISSIONS})
            elseif (DEFINED COR_INSTALL_DEFAULT_PERMISSIONS)
                set(PERMISSIONS ${COR_INSTALL_DEFAULT_PERMISSIONS})
            else()
                # Directories need OWNER_EXECUTE in order to be deletable by owner
                set(PERMISSIONS ${DEFAULT_PERMISSIONS} OWNER_EXECUTE)
            endif()

            if (DEFINED COR_INSTALL_PUBLIC_HEADER_CONFIGURATIONS)
                set(CONFIGURATIONS CONFIGURATIONS ${COR_INSTALL_PUBLIC_HEADER_CONFIGURATIONS})
            elseif (DEFINED COR_INSTALL_DEFAULT_CONFIGURATIONS)
                set(CONFIGURATIONS CONFIGURATIONS ${COR_INSTALL_DEFAULT_CONFIGURATIONS})
            else()
                set(CONFIGURATIONS)
            endif()

            get_target_property(FILE_SET ${INSTALL_TARGET} INTERFACE_HEADER_SETS)
            if(NOT FILE_SET OR FILE_SET MATCHES .*-NOTFOUND)
                set(TARGET_HAS_FILE_SET FALSE)
            else()
                set(TARGET_HAS_FILE_SET TRUE)
            endif()

            if(NOT TARGET_HAS_FILE_SET)
                if(EXPORT_NAME)
                    # We still need to generate a EXPORT but we can't do that with install(DIRECTORY)
                    install(TARGETS ${INSTALL_TARGET} ${EXPORT_NAME})
                endif()

                set(PUBLIC_HEADER_PROPERTIES INCLUDE_DIRECTORIES PUBLIC_INCLUDE_DIRECTORIES INTERFACE_INCLUDE_DIRECTORIES)
                foreach(PUBLIC_HEADER_PROPERTY ${PUBLIC_HEADER_PROPERTIES})
                    get_target_property(PUBLIC_HEADER ${INSTALL_TARGET} ${PUBLIC_HEADER_PROPERTY})

                    if(NOT PUBLIC_HEADER MATCHES .*-NOTFOUND)
                        foreach(INCLUDE_DIRECTORY ${PUBLIC_HEADER})
                            install(
                                    DIRECTORY ${INCLUDE_DIRECTORY}
                                    DESTINATION .
                                    FILE_PERMISSIONS ${PERMISSIONS}
                                    DIRECTORY_PERMISSIONS ${PERMISSIONS}
                                    ${CONFIGURATIONS}
                            )
                        endforeach()
                    endif()
                endforeach()
            else()
                install(
                        TARGETS ${INSTALL_TARGET}
                        ${EXPORT_NAME}
                        FILE_SET HEADERS
                        DESTINATION ${DESTINATION}
                        PERMISSIONS ${PERMISSIONS}
                        ${CONFIGURATIONS}
                )
            endif()
        endforeach()

    elseif(INSTALL_TYPE STREQUAL "EXPORT")
        message(FATAL_ERROR "install(EXPORT ...) not yet implemented")
    else()
        message(FATAL_ERROR "Unknown arg: ${INSTALL_TYPE}")
    endif()
endfunction()

function(_corrosion_check_cxx_version_helper manifest_dir cxx_name out_required_version)
    execute_process(COMMAND ${CMAKE_COMMAND} -E env
                    "CARGO_BUILD_RUSTC=${_CORROSION_RUSTC}"
                    ${_CORROSION_CARGO} tree -i "${cxx_name}"
                    # Usage of `cxx` could be gated behind a feature. Features can use Generator expressions,
                    # so we can't really know what features we will enable when building at this point.
                    # Features should be additive though, so simply enabling all-features should work for
                    # dependency resolution.
                    --all-features
                    --target all
                    --depth=0
                    WORKING_DIRECTORY "${manifest_dir}"
                    RESULT_VARIABLE cxx_version_result
                    OUTPUT_VARIABLE cxx_version_output
                    ERROR_VARIABLE cxx_version_error
    )
    if(NOT "${cxx_version_result}" EQUAL "0")
        message(DEBUG "`cargo tree -i ${cxx_name}` returned an error: ${cxx_version_error}")
        set("${out_required_version}" "${cxx_name}-NOTFOUND" PARENT_SCOPE)
        return()
    endif()
    if(cxx_version_output MATCHES "${cxx_name} v([0-9]+.[0-9]+.[0-9]+)")
        set("${out_required_version}" "${CMAKE_MATCH_1}" PARENT_SCOPE)
    else()
        message(DEBUG "Failed to parse `cargo tree -i ${cxx_name}` output: ${cxx_version_output}")
        set("${out_required_version}" "${cxx_name}-NOTFOUND" PARENT_SCOPE)
    endif()
endfunction()

function(_corrosion_check_cxx_version manifest_dir out_required_version)
    # cxxbridge-cmd is known to be available in lockfiles since cxx 1.0.131.
    # We include `cxx` as a fallback to support older versions too. `cxxbridge` should always
    # be exactly the same version as `cxx`, so falling back to `cxx` version should not cause issues.
    foreach(cxxbridge_name cxxbridge-cmd cxx)
        unset(cxx_required_version)
        _corrosion_check_cxx_version_helper("${manifest_dir}"
                                            "${cxxbridge_name}"
                                            cxx_required_version)
        if(cxx_required_version)
            set("${out_required_version}" "${cxx_required_version}" PARENT_SCOPE)
            break()
        else()
            set("${out_required_version}" "cxx-NOTFOUND" PARENT_SCOPE)
        endif()
    endforeach()

endfunction()



#[=======================================================================[.md:
** EXPERIMENTAL **: This function is currently still considered experimental
  and is not officially released yet. Feedback and Suggestions are welcome.

ANCHOR: corrosion_add_cxxbridge

```cmake
corrosion_add_cxxbridge(cxx_target
        CRATE <imported_target_name>
        REGEN_TARGET <regen_target_name>
        [FILES <file1.rs> <file2.rs>]
)
```

Adds build-rules to create C++ bindings using the [cxx] crate.

### Arguments:
* `cxxtarget`: Name of the C++ library target for the bindings, which corrosion will create.
* **FILES**: Input Rust source file containing #[cxx::bridge].
* **CRATE**: Name of an imported Rust target. Note: Parameter may be renamed before release
* **REGEN_TARGET**: Name of a custom target that will regenerate the cxx bindings **without** recompiling. Note: Parameter may be renamed before release

#### Currently missing arguments

The following arguments to cxxbridge **currently** have no way to be passed by the user:
- `--cfg`
- `--cxx-impl-annotations`
- `--include`

The created rules approximately do the following:
- Check which version of `cxx` the Rust crate specified by the `CRATE` argument depends on.
- Check if the exact same version of `cxxbridge-cmd` is installed (available in `PATH`)
- If not, create a rule to build the exact same version of `cxxbridge-cmd`.
- Create rules to run `cxxbridge` and generate
  - The `rust/cxx.h` header
  - A header and source file for each of the files specified in `FILES`
- The generated sources (and header include directories) are added to the `cxxtarget` CMake
  library target.

### Limitations

We currently require the `CRATE` argument to be a target imported by Corrosion, however,
Corrosion does not import `rlib` only libraries. As a workaround users can add
`staticlib` to their list of crate kinds. In the future this may be solved more properly,
by either adding an option to also import Rlib targets (without build rules) or by
adding a `MANIFEST_PATH` argument to this function, specifying where the crate is.

### Contributing

Specifically some more realistic test / demo projects and feedback about limitations would be
welcome.

[cxx]: https://github.com/dtolnay/cxx

ANCHOR_END: corrosion_add_cxxbridge
#]=======================================================================]
function(corrosion_add_cxxbridge cxx_target)
    set(OPTIONS)
    set(ONE_VALUE_KEYWORDS CRATE REGEN_TARGET)
    set(MULTI_VALUE_KEYWORDS FILES)
    cmake_parse_arguments(PARSE_ARGV 1 _arg "${OPTIONS}" "${ONE_VALUE_KEYWORDS}" "${MULTI_VALUE_KEYWORDS}")

    set(required_keywords CRATE FILES)
    foreach(keyword ${required_keywords})
        if(NOT DEFINED "_arg_${keyword}")
            message(FATAL_ERROR "Missing required parameter `${keyword}`.")
        elseif("${_arg_${keyword}}" STREQUAL "")
            message(FATAL_ERROR "Required parameter `${keyword}` may not be set to an empty string.")
        endif()
    endforeach()

    if(DEFINED _arg_UNPARSED_ARGUMENTS)
        message(AUTHOR_WARNING "corrosion_add_cxxbridge was called with the following unknown arguments: "
                "`${_arg_UNPARSED_ARGUMENTS}`\n"
                "Unknown arguments will be ignored."
        )
    endif()

    get_target_property(manifest_path "${_arg_CRATE}" INTERFACE_COR_PACKAGE_MANIFEST_PATH)

    if(NOT EXISTS "${manifest_path}")
        message(FATAL_ERROR "Internal error: No package manifest found at ${manifest_path}")
    endif()

    get_filename_component(manifest_dir ${manifest_path} DIRECTORY)

    _corrosion_check_cxx_version("${manifest_dir}" cxx_required_version)

    if(NOT cxx_required_version)
        message(FATAL_ERROR
                "Failed to find a dependency on `cxxbridge-cmd` / `cxx` for crate ${_arg_CRATE}"
        )
    endif()

    # First check if a suitable version of cxxbridge is installed
    find_program(INSTALLED_CXXBRIDGE cxxbridge PATHS "$ENV{HOME}/.cargo/bin/")
    mark_as_advanced(INSTALLED_CXXBRIDGE)
    if(INSTALLED_CXXBRIDGE)
        execute_process(COMMAND ${INSTALLED_CXXBRIDGE} --version OUTPUT_VARIABLE cxxbridge_version_output)
        if(cxxbridge_version_output MATCHES "cxxbridge ([0-9]+.[0-9]+.[0-9]+)")
            set(cxxbridge_version "${CMAKE_MATCH_1}")
        else()
            set(cxxbridge_version "")
        endif()
    endif()

    set(cxxbridge "")
    if(cxxbridge_version)
        if(cxxbridge_version VERSION_EQUAL cxx_required_version)
            set(cxxbridge "${INSTALLED_CXXBRIDGE}")
            if(NOT TARGET "cxxbridge_v${cxx_required_version}")
                # Add an empty target.
                add_custom_target("cxxbridge_v${cxx_required_version}"
                    )
            endif()
        endif()
    endif()

    # No suitable version of cxxbridge was installed, so use custom target to build correct version.
    if(NOT cxxbridge)
        if(NOT TARGET "cxxbridge_v${cxx_required_version}")
            unset(executable_postfix)
            if(Rust_CARGO_HOST_OS STREQUAL "windows")
                set(executable_postfix ".exe")
            endif()

            add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/corrosion/cxxbridge_v${cxx_required_version}/bin/cxxbridge${executable_postfix}"
                COMMAND
                ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/corrosion/cxxbridge_v${cxx_required_version}"
                COMMAND
                    ${CMAKE_COMMAND} -E env
                        "CARGO_BUILD_RUSTC=$CACHE{CORROSION_TOOLS_RUSTC}"
                    $CACHE{CORROSION_TOOLS_CARGO} install
                    cxxbridge-cmd
                    --version "${cxx_required_version}"
                    --locked
                    --root "${CMAKE_BINARY_DIR}/corrosion/cxxbridge_v${cxx_required_version}"
                    --quiet
                    # todo: use --target-dir to potentially reuse artifacts
                COMMENT "Building cxxbridge (version ${cxx_required_version}) with Rust toolchain $CACHE{CORROSION_TOOLS_RUST_TOOLCHAIN}"
                )
            add_custom_target("cxxbridge_v${cxx_required_version}"
                DEPENDS "${CMAKE_BINARY_DIR}/corrosion/cxxbridge_v${cxx_required_version}/bin/cxxbridge${executable_postfix}"
                )
        endif()
        set(cxxbridge "${CMAKE_BINARY_DIR}/corrosion/cxxbridge_v${cxx_required_version}/bin/cxxbridge${executable_postfix}")
    endif()


    # The generated folder structure will be of the following form
    #
    #    CMAKE_CURRENT_BINARY_DIR
    #        corrosion_generated
    #            cxxbridge
    #                <cxx_target>
    #                    include
    #                        <cxx_target>
    #                            <headers>
    #                        rust
    #                            cxx.h
    #                    src
    #                        <sourcefiles>
    #            cbindgen
    #                ...
    #            other
    #                ...

    set(corrosion_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/corrosion_generated")
    set(generated_dir "${corrosion_generated_dir}/cxxbridge/${cxx_target}")
    set(header_placement_dir "${generated_dir}/include/${cxx_target}")
    set(source_placement_dir "${generated_dir}/src")

    add_library(${cxx_target} STATIC)
    target_include_directories(${cxx_target}
        PUBLIC
            $<BUILD_INTERFACE:${generated_dir}/include>
            $<INSTALL_INTERFACE:include>
    )

    # cxx generated code is using c++11 features in headers, so propagate c++11 as minimal requirement
    target_compile_features(${cxx_target} PUBLIC cxx_std_11)

    if (TARGET "${_arg_CRATE}-static")
        target_link_libraries(${cxx_target} PRIVATE "${_arg_CRATE}-static")
        target_link_libraries("${_arg_CRATE}-static" INTERFACE ${cxx_target})
    endif()
    if (TARGET "${_arg_CRATE}-shared")
        target_link_libraries(${cxx_target} PRIVATE "${_arg_CRATE}-shared")
        target_link_libraries("${_arg_CRATE}-shared" INTERFACE ${cxx_target})
    endif()

    file(MAKE_DIRECTORY "${generated_dir}/include/rust")
    add_custom_command(
            OUTPUT "${generated_dir}/include/rust/cxx.h"
            COMMAND
            ${cxxbridge} --header --output "${generated_dir}/include/rust/cxx.h"
            DEPENDS "cxxbridge_v${cxx_required_version}"
            COMMENT "Generating rust/cxx.h header"
    )

    set(GENERATED_SOURCES "")
    set(GENERATED_HEADERS "${generated_dir}/include/rust/cxx.h")

    foreach(filepath ${_arg_FILES})
        get_filename_component(filename ${filepath} NAME_WE)
        get_filename_component(directory ${filepath} DIRECTORY)
        set(directory_component "")
        if(directory)
            set(directory_component "${directory}/")
        endif()
        # todo: convert potentially absolute paths to relative paths..
        set(cxx_header ${directory_component}${filename}.h)
        set(cxx_source ${directory_component}${filename}.cpp)

        # todo: not all projects may use the `src` directory.
        set(rust_source_path "${manifest_dir}/src/${filepath}")

        file(MAKE_DIRECTORY "${header_placement_dir}/${directory}" "${source_placement_dir}/${directory}")

        add_custom_command(
            OUTPUT
            "${header_placement_dir}/${cxx_header}"
            "${source_placement_dir}/${cxx_source}"
            COMMAND
                ${cxxbridge} ${rust_source_path} --header --output "${header_placement_dir}/${cxx_header}"
            COMMAND
                ${cxxbridge} ${rust_source_path}
                    --output "${source_placement_dir}/${cxx_source}"
                    --include "${cxx_target}/${cxx_header}"
            DEPENDS "cxxbridge_v${cxx_required_version}" "${rust_source_path}"
            COMMENT "Generating cxx bindings for crate ${_arg_CRATE} and file src/${filepath}"
        )

        list(APPEND GENERATED_SOURCES "${source_placement_dir}/${cxx_source}")
        list(APPEND GENERATED_HEADERS "${header_placement_dir}/${cxx_header}")
    endforeach()
    target_sources(${cxx_target} PRIVATE ${GENERATED_SOURCES})
    # Make sure to export the headers with PUBLIC.
    # This ensures that any target that depends on cxx_target also has these files as a dependency
    # CMake will then make sure to generate the files before building either target, which is important
    # in the presence of circular dependencies
    target_sources(${cxx_target} PUBLIC ${GENERATED_HEADERS})

    if(DEFINED _arg_REGEN_TARGET)
        # Add only the headers to the regen target, as the sources are actually not needed
        # For the IDE to pick everything up
        add_custom_target(${_arg_REGEN_TARGET}
            DEPENDS ${GENERATED_HEADERS}
            COMMENT "Generated cxx bindings for crate ${_arg_CRATE}")
    endif()

endfunction()

#[=======================================================================[.md:
ANCHOR: corrosion_cbindgen

A helper function which uses [cbindgen] to generate C/C++ bindings for a Rust crate.
If `cbindgen` is not in `PATH` the helper function will automatically try to download
`cbindgen` and place the built binary into `CMAKE_BINARY_DIR`. The binary is shared
between multiple invocations of this function.

The function comes with two different signatures. It's recommended to use the `TARGET` based signature when possible.

### Auto mode (With a Rust target imported by corrosion)
```cmake
corrosion_experimental_cbindgen(
        TARGET <imported_target_name>
        HEADER_NAME <output_header_name>
        [CBINDGEN_VERSION <version>]
        [FLAGS <flag1> ... <flagN>]
)
```

### Auto-mode specific Arguments


* **TARGET**: The name of an imported Rust library target, for which bindings should be generated.
              If the target is not imported by Corrosion, because the crate only produces an
              `rlib`, you can instead use the second signature and manually pass `MANIFEST_DIRECTORY`,
              `CARGO_PACKAGE` and `BINDINGS_TARGET`

### Manual mode (Without a Rust target imported by corrosion)
```cmake
corrosion_experimental_cbindgen(
        MANIFEST_DIRECTORY <package_manifest_directory>
        CARGO_PACKAGE <package_name>
        BINDINGS_TARGET <cmake_library>
        [TARGET_TRIPLE <rust_target_triple>]
        HEADER_NAME <output_header_name>
        [CBINDGEN_VERSION <version>]
        [FLAGS <flag1> ... <flagN>]
)
```

### Manual-mode specific Arguments

* **MANIFEST_DIRECTORY**: Manual mode only.
    Directory of the package defining the library crate bindings should be generated for.
    If you want to avoid specifying `MANIFEST_DIRECTORY` you could add a `staticlib` target to your package
    manifest as a workaround to make corrosion import the crate.

* **CARGO_PACKAGE**: Manual mode only.
    The name of the cargo package that bindings should be generated for.
    Note: This corresponds to the `cbindgen` `--crate` option, which actually wants a package name.

* **BINDINGS_TARGET**: Manual mode only.
    Name of an `INTERFACE` CMake target that the generated bindings should be attached to.
    In auto mode, the generated headers will be attached to the imported rust CMake crate,
    and corrosion will take care of adding the necessary build dependencies.
    In manual mode, this target likely doesn't exist, so the user needs to specify an INTERFACE CMake
    target, which the header files should be attached to. The user must create this target themselves and
    ensure to add any necessary dependencies (e.g. via `add_dependencies()`) to ensure that consumers of the
    `INTERFACE` library are not linked before the Rust library has been built.

* **TARGET_TRIPLE**: Manual mode only.
    Rust target triple (e.g. `x86_64-unknown-linux-gnu`) that cbindgen should use when generating the bindings.
    Defaults to target triple that corrosion was confiured to compile for.

### Common Arguments

* **HEADER_NAME**: The name of the generated header file. This will be the name which you include in your C/C++ code
                    (e.g. `#include "myproject/myheader.h" if you specify `HEADER_NAME "myproject/myheader.h"`.
* **CBINDGEN_VERSION**: Version requirement for cbindgen. Exact semantics to be specified. Currently not implemented.
* **FLAGS**: Arbitrary other flags for `cbindgen`. Run `cbindgen --help` to see the possible flags.

[cbindgen]: https://github.com/mozilla/cbindgen

### Current limitations

- Cbindgens (optional) macro expansion feature internally actually builds the crate / runs the build script.
  For this to work as expected in all cases, we probably need to set all the same environment variables
  as when corrosion builds the crate. However the crate is a **library**, so we would need to figure out which
  target builds it - and if there are multiple, potentially generate bindings per-target?
  Alternatively we could add support of setting some environment variables on rlibs, and pulling that
  information in when building the actual corrosion targets
  Alternatively we could restrict corrosions support of this feature to actual imported staticlib/cdylib targets.
ANCHOR_END: corrosion_cbindgen
#]=======================================================================]
function(corrosion_experimental_cbindgen)
    set(OPTIONS "")
    set(ONE_VALUE_KEYWORDS
            TARGET
            MANIFEST_DIRECTORY
            CARGO_PACKAGE
            BINDINGS_TARGET
            TARGET_TRIPLE
            HEADER_NAME
            CBINDGEN_VERSION
    )
    set(MULTI_VALUE_KEYWORDS "FLAGS")
    cmake_parse_arguments(PARSE_ARGV 0 CCN "${OPTIONS}" "${ONE_VALUE_KEYWORDS}" "${MULTI_VALUE_KEYWORDS}")

    set(required_keywords HEADER_NAME)
    foreach(keyword ${required_keywords})
        if(NOT DEFINED "CCN_${keyword}")
            message(FATAL_ERROR "Missing required parameter `${keyword}`.")
        elseif("${CCN_${keyword}}" STREQUAL "")
            message(FATAL_ERROR "Required parameter `${keyword}` may not be set to an empty string.")
        endif()
    endforeach()
    if(NOT (DEFINED CCN_TARGET
            OR (DEFINED CCN_MANIFEST_DIRECTORY AND DEFINED CCN_BINDINGS_TARGET
                AND DEFINED CCN_BINDINGS_TARGET)
            )
    )
        message(FATAL_ERROR "Unknown signature for corrosion_experimental_cbindgen.\n"
                "Either the `TARGET` or the `MANIFEST_DIRECTORY` based signature must be chosen.\n"
                "Please view the documentation for details on the function signature.\n"
                "Passed arguments where: `${ARGV}`"
        )
    endif()

    if(DEFINED CCN_UNPARSED_ARGUMENTS)
        message(AUTHOR_WARNING "corrosion_experimental_cbindgen was called with the following unknown arguments: "
                "`${CCN_UNPARSED_ARGUMENTS}`\n"
                "Unknown arguments will be ignored."
        )
    endif()
    unset(package_manifest_dir)


    if(TARGET "${CCN_TARGET}")
        set(cbindgen_bindings_target "${CCN_TARGET}")
        set(hostbuild_override "$<BOOL:$<TARGET_PROPERTY:${CCN_TARGET},${_CORR_PROP_HOST_BUILD}>>")
        set(cbindgen_target_triple "$<IF:${hostbuild_override},${_CORROSION_RUST_CARGO_HOST_TARGET},${_CORROSION_RUST_CARGO_TARGET}>")

        get_target_property(package_manifest_path "${CCN_TARGET}" INTERFACE_COR_PACKAGE_MANIFEST_PATH)
        if(NOT EXISTS "${package_manifest_path}")
            message(FATAL_ERROR "Internal error: No package manifest found at ${package_manifest_path}")
        endif()
        get_filename_component(package_manifest_dir "${package_manifest_path}" DIRECTORY)
        get_target_property(rust_cargo_package "${CCN_TARGET}" COR_CARGO_PACKAGE_NAME )
        if(NOT rust_cargo_package)
            message(FATAL_ERROR "Internal Error: Could not determine cargo package name for cbindgen. ")
        endif()
        # todo: as an optimization we could cache the cargo metadata output (but --no-deps makes that slightly more complicated)
    else()
        if(NOT DEFINED CCN_MANIFEST_DIRECTORY)
            message(FATAL_ERROR
                "Internal error: There should have been a fatal error already if neither TARGET or "
                    "MANIFEST_DIRECTORY are specfied.")
        endif()
        cmake_path(ABSOLUTE_PATH CCN_MANIFEST_DIRECTORY NORMALIZE OUTPUT_VARIABLE package_manifest_dir)
        if(DEFINED CCN_TARGET_TRIPLE)
            set(cbindgen_target_triple "${CCN_TARGET_TRIPLE}")
        else()
            set(cbindgen_target_triple "${Rust_CARGO_TARGET}")
        endif()
        set(rust_cargo_package "${CCN_CARGO_PACKAGE}")
        set(cbindgen_bindings_target "${CCN_BINDINGS_TARGET}")
        get_target_property(type "${cbindgen_bindings_target}" TYPE)
        if(NOT ${type} STREQUAL "INTERFACE_LIBRARY")
            message(AUTHOR_WARNING "The CMake target for the cbindgen generated files is expected to be"
                " an `INTERFACE` library, but was `${type}` instead."
            )
        endif()
    endif()

    message(STATUS "Using package `${rust_cargo_package}` as crate for cbindgen")

    set(output_header_name "${CCN_HEADER_NAME}")

    find_program(installed_cbindgen cbindgen)

    # Install the newest cbindgen version into our build tree.
    if(installed_cbindgen)
        set(cbindgen "${installed_cbindgen}")
    else()
        set(local_cbindgen_install_dir "${CMAKE_BINARY_DIR}/corrosion/cbindgen")
        unset(executable_postfix)
        if(Rust_CARGO_HOST_OS STREQUAL "windows")
            set(executable_postfix ".exe")
        endif()
        set(cbindgen "${local_cbindgen_install_dir}/bin/cbindgen${executable_postfix}")

        if(NOT TARGET "_corrosion_cbindgen")
            file(MAKE_DIRECTORY "${local_cbindgen_install_dir}")

            add_custom_command(OUTPUT "${cbindgen}"
                COMMAND ${CMAKE_COMMAND}
                -E env
                "CARGO_BUILD_RUSTC=$CACHE{CORROSION_TOOLS_RUSTC}"
                $CACHE{CORROSION_TOOLS_CARGO} install
                    cbindgen
                    --locked
                    --root "${local_cbindgen_install_dir}"
                    ${_CORROSION_QUIET_OUTPUT_FLAG}
                COMMENT "Building cbindgen with Rust toolchain $CACHE{CORROSION_TOOLS_RUST_TOOLCHAIN}"
                VERBATIM
                )
            add_custom_target("_corrosion_cbindgen"
                DEPENDS "${cbindgen}"
                )
        endif()
    endif()

    set(corrosion_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/corrosion_generated")
    set(generated_dir "${corrosion_generated_dir}/cbindgen/${cbindgen_bindings_target}")
    set(header_placement_dir "${generated_dir}/include")
    set(depfile_placement_dir "${generated_dir}/depfile")
    set(generated_depfile "${depfile_placement_dir}/${output_header_name}.d")
    set(generated_header "${header_placement_dir}/${output_header_name}")

    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.23")
        target_sources(${cbindgen_bindings_target}
            INTERFACE
            FILE_SET HEADERS
            BASE_DIRS "${header_placement_dir}"
            FILES "${header_placement_dir}/${output_header_name}"
        )
    else()
        # Note: not clear to me how install would best work before CMake 3.23
        target_include_directories(${cbindgen_bindings_target}
            INTERFACE
            $<BUILD_INTERFACE:${header_placement_dir}>
            $<INSTALL_INTERFACE:include>
            )
    endif()

    # This may be different from $header_placement_dir since the user specified HEADER_NAME may contain
    # relative directories.
    get_filename_component(generated_header_dir "${generated_header}" DIRECTORY)
    file(MAKE_DIRECTORY "${generated_header_dir}")

    unset(depfile_cbindgen_arg)
    get_filename_component(generated_depfile_dir "${generated_depfile}" DIRECTORY)
    file(MAKE_DIRECTORY "${generated_depfile_dir}")
    set(depfile_cbindgen_arg "--depfile=${generated_depfile}")

    add_custom_command(
        OUTPUT
        "${generated_header}"
        COMMAND
        "${CMAKE_COMMAND}" -E env
            TARGET="${cbindgen_target_triple}"
            # cbindgen invokes cargo-metadata and checks the CARGO environment variable
            CARGO="${_CORROSION_CARGO}"
            RUSTC="${_CORROSION_RUSTC}"
            "${cbindgen}"
                    --output "${generated_header}"
                    --crate "${rust_cargo_package}"
                    ${depfile_cbindgen_arg}
                    ${CCN_FLAGS}
        COMMENT "Generate cbindgen bindings for package ${rust_cargo_package} and output header ${generated_header}"
        DEPFILE "${generated_depfile}"
        COMMAND_EXPAND_LISTS
        WORKING_DIRECTORY "${package_manifest_dir}"
    )

    if(NOT installed_cbindgen)
        add_custom_command(
            OUTPUT "${generated_header}"
            APPEND
            DEPENDS _corrosion_cbindgen
        )
    endif()

    if(NOT TARGET "_corrosion_cbindgen_${cbindgen_bindings_target}_bindings")
        add_custom_target(_corrosion_cbindgen_${cbindgen_bindings_target}_bindings
                COMMENT "Generate cbindgen bindings for package ${rust_cargo_package}"
        )
    endif()
    # Users might want to call cbindgen multiple times, e.g. to generate separate C++ and C header files.
    string(MAKE_C_IDENTIFIER "${output_header_name}" header_identifier )
    add_custom_target("_corrosion_cbindgen_${cbindgen_bindings_target}_bindings_${header_identifier}"
            DEPENDS "${generated_header}"
            COMMENT "Generate ${generated_header} for ${cbindgen_bindings_target}"
    )
    add_dependencies("_corrosion_cbindgen_${cbindgen_bindings_target}_bindings" "_corrosion_cbindgen_${cbindgen_bindings_target}_bindings_${header_identifier}")
    add_dependencies(${cbindgen_bindings_target} "_corrosion_cbindgen_${cbindgen_bindings_target}_bindings")
    if(TARGET "${CCN_TARGET}")
        add_dependencies(cargo-build_${CCN_TARGET} "_corrosion_cbindgen_${cbindgen_bindings_target}_bindings")
    endif()
endfunction()

# Parse the version of a Rust package from it's package manifest (Cargo.toml)
function(corrosion_parse_package_version package_manifest_path out_package_version)
    if(NOT EXISTS "${package_manifest_path}")
        message(FATAL_ERROR "Package manifest `${package_manifest_path}` does not exist.")
    endif()

    file(READ "${package_manifest_path}" package_manifest)

    # Find the package table. It may contain arrays, so match until \n\[, which should mark the next
    # table. Note: backslashes must be doubled to escape the backslash for the bracket. LF is single
    # backslash however. On windows the line also ends in \n, so matching against \n\[ is sufficient
    # to detect an opening bracket on a new line.
    set(package_table_regex "\\[package\\](.*)\n\\[")

    string(REGEX MATCH "${package_table_regex}" _package_table "${package_manifest}")

    if(CMAKE_MATCH_COUNT EQUAL "1")
        set(package_table "${CMAKE_MATCH_1}")
    else()
        message(DEBUG
                "Failed to find `[package]` table in package manifest `${package_manifest_path}`.\n"
                "Matches: ${CMAKE_MATCH_COUNT}\n"
        )
        set(${out_package_version}
            "NOTFOUND"
            PARENT_SCOPE
        )
    endif()
    # Match `version = "0.3.2"`, `"version" = "0.3.2" Contains one matching group for the version
    set(version_regex "[\r]?\n[\"']?version[\"']?[ \t]*=[ \t]*[\"']([0-9\.]+)[\"']")

    string(REGEX MATCH "${version_regex}" _version "${package_table}")

    if("${package_table}" MATCHES "${version_regex}")
        set(${out_package_version}
            "${CMAKE_MATCH_1}"
            PARENT_SCOPE
        )
    else()
        message(DEBUG "Failed to extract package version from manifest `${package_manifest_path}`.")
        set(${out_package_version}
            "NOTFOUND"
            PARENT_SCOPE
        )
    endif()
endfunction()

function(_corrosion_initialize_properties target_name)
    # Initialize the `<XYZ>_OUTPUT_DIRECTORY` properties based on `CMAKE_<XYZ>_OUTPUT_DIRECTORY`.
    foreach(output_var RUNTIME_OUTPUT_DIRECTORY ARCHIVE_OUTPUT_DIRECTORY LIBRARY_OUTPUT_DIRECTORY PDB_OUTPUT_DIRECTORY)
        if (DEFINED "CMAKE_${output_var}")
            set_property(TARGET ${target_name} PROPERTY "${output_var}" "${CMAKE_${output_var}}")
        endif()

        foreach(config_type ${CMAKE_CONFIGURATION_TYPES})
            string(TOUPPER "${config_type}" config_type_upper)
            if (DEFINED "CMAKE_${output_var}_${config_type_upper}")
                set_property(TARGET ${target_name} PROPERTY "${output_var}_${config_type_upper}" "${CMAKE_${output_var}_${config_type_upper}}")
            endif()
        endforeach()
    endforeach()
endfunction()

# Helper macro to pass through an optional `OPTION` argument parsed via `cmake_parse_arguments`
# to another function that takes the same OPTION.
# If the option was set, then the variable <var_name> will be set to the same option name again,
# otherwise <var_name> will be unset.
macro(_corrosion_option_passthrough_helper option_name prefix var_name)
    if(${${prefix}_${option_name}})
        set("${var_name}" "${option_name}")
    else()
        unset("${var_name}")
    endif()
endmacro()

# Helper macro to pass through an optional argument with value(s), parsed via `cmake_parse_arguments`,
# to another function that takes the same keyword + associated values.
# If the argument was given, then the variable <var_name> will be a list of the argument name and the values,
# which will be expanded, when calling the function (assuming no quotes).
macro(_corrosion_arg_passthrough_helper arg_name prefix var_name)
    if(DEFINED "${prefix}_${arg_name}")
        set("${var_name}" "${arg_name}" "${${prefix}_${arg_name}}")
    else()
        unset("${var_name}")
    endif()
endmacro()

list(POP_BACK CMAKE_MESSAGE_CONTEXT)
