function(_cargo_metadata out manifest)
    set(OPTIONS LOCKED FROZEN)
    set(ONE_VALUE_KEYWORDS "")
    set(MULTI_VALUE_KEYWORDS "")
    cmake_parse_arguments(PARSE_ARGV 2 CM "${OPTIONS}" "${ONE_VALUE_KEYWORDS}" "${MULTI_VALUE_KEYWORDS}")

    list(APPEND CMAKE_MESSAGE_CONTEXT "_cargo_metadata")

    if(DEFINED CM_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Internal error - unexpected arguments: ${CM_UNPARSED_ARGUMENTS}")
    elseif(DEFINED CM_KEYWORDS_MISSING_VALUES)
        message(FATAL_ERROR "Internal error - the following keywords had no associated value(s):"
            "${CM_KEYWORDS_MISSING_VALUES}")
    endif()

    set(cargo_locked "")
    set(cargo_frozen "")
    if(LOCKED)
        set(cargo_locked "--locked")
    endif()
    if(FROZEN)
        set(cargo_frozen "--frozen")
    endif()
    
    get_filename_component(workspace_toml_dir "${manifest}" DIRECTORY)
    
    execute_process(
        COMMAND
            ${CMAKE_COMMAND} -E env
                "CARGO_BUILD_RUSTC=${_CORROSION_RUSTC}"
                "${_CORROSION_CARGO}"
                    metadata
                        --manifest-path "${manifest}"
                        --format-version 1
                        # We don't care about non-workspace dependencies
                        --no-deps
                        ${cargo_locked}
                        ${cargo_frozen}
        # Set WORKING_DIRECTORY to the directory containing the manifest, so that configuration files
        # such as `.cargo/config.toml` or `toolchain.toml` are applied as expected. Cargo searches for
        # configuration files by walking upward from the current directory.
        WORKING_DIRECTORY "${workspace_toml_dir}"
        OUTPUT_VARIABLE json
        COMMAND_ERROR_IS_FATAL ANY
    )

    set(${out} "${json}" PARENT_SCOPE)
endfunction()

# Add targets (crates) of one package
function(_generator_add_package_targets)
    set(OPTIONS NO_LINKER_OVERRIDE)
    set(ONE_VALUE_KEYWORDS
        WORKSPACE_MANIFEST_PATH
        PACKAGE_MANIFEST_PATH
        PACKAGE_NAME
        PACKAGE_VERSION
        TARGETS_JSON
        OUT_CREATED_TARGETS)
    set(MULTI_VALUE_KEYWORDS CRATE_TYPES OVERRIDE_CRATE_TYPE_ARGS)
    cmake_parse_arguments(PARSE_ARGV 0 GAPT "${OPTIONS}" "${ONE_VALUE_KEYWORDS}" "${MULTI_VALUE_KEYWORDS}")

    if(DEFINED GAPT_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Internal error - unexpected arguments: ${GAPT_UNPARSED_ARGUMENTS}")
    elseif(DEFINED GAPT_KEYWORDS_MISSING_VALUES)
        message(FATAL_ERROR "Internal error - the following keywords had no associated value(s):"
                    "${GAPT_KEYWORDS_MISSING_VALUES}")
    endif()

    _corrosion_option_passthrough_helper(NO_LINKER_OVERRIDE GAPT no_linker_override)

    set(workspace_manifest_path "${GAPT_WORKSPACE_MANIFEST_PATH}")
    set(package_manifest_path "${GAPT_PACKAGE_MANIFEST_PATH}")
    set(package_name "${GAPT_PACKAGE_NAME}")
    set(package_version "${GAPT_PACKAGE_VERSION}")
    set(targets "${GAPT_TARGETS_JSON}")
    set(out_created_targets "${GAPT_OUT_CREATED_TARGETS}")
    set(crate_types "${GAPT_CRATE_TYPES}")
    if(DEFINED GAPT_OVERRIDE_CRATE_TYPE_ARGS)
        list(GET GAPT_OVERRIDE_CRATE_TYPE_ARGS 0 override_crate_name_list_ref)
        list(GET GAPT_OVERRIDE_CRATE_TYPE_ARGS 1 override_crate_types_list_ref)
    endif()

    set(corrosion_targets "")

    file(TO_CMAKE_PATH "${package_manifest_path}" manifest_path)

    string(JSON targets_len LENGTH "${targets}")
    math(EXPR targets_len-1 "${targets_len} - 1")

    message(DEBUG "Found ${targets_len} targets in package ${package_name}")

    foreach(ix RANGE ${targets_len-1})
        string(JSON target GET "${targets}" ${ix})
        string(JSON target_name GET "${target}" "name")
        string(JSON target_kind GET "${target}" "kind")
        string(JSON target_kind_len LENGTH "${target_kind}")

        math(EXPR target_kind_len-1 "${target_kind_len} - 1")
        set(kinds)
        unset(override_package_crate_type)
        # OVERRIDE_CRATE_TYPE is more specific than the CRATE_TYPES argument to corrosion_import_crate, and thus takes
        # priority.
        if(DEFINED GAPT_OVERRIDE_CRATE_TYPE_ARGS)
            foreach(override_crate_name override_crate_types IN ZIP_LISTS ${override_crate_name_list_ref} ${override_crate_types_list_ref})
                if("${override_crate_name}" STREQUAL "${target_name}")
                    message(DEBUG "Forcing crate ${target_name} to crate-type(s): ${override_crate_types}.")
                    # Convert to CMake list
                    string(REPLACE "," ";" kinds "${override_crate_types}")
                    break()
                endif()
            endforeach()
        else()
            foreach(ix RANGE ${target_kind_len-1})
                string(JSON kind GET "${target_kind}" ${ix})
                if(NOT crate_types OR ${kind} IN_LIST crate_types)
                    list(APPEND kinds ${kind})
                endif()
            endforeach()
        endif()
        if(TARGET "${target_name}"
            AND ("staticlib" IN_LIST kinds OR "cdylib" IN_LIST kinds OR "bin" IN_LIST kinds)
            )
            message(WARNING "Failed to import Rust crate ${target_name} (kind: `${target_kind}`) because a target "
                "with the same name already exists. Skipping this target.\n"
                "Help: If you are importing a package which exposes both a `lib` and "
                "a `bin` target, please consider explicitly naming the targets in your `Cargo.toml` manifest.\n"
                "Note: If you have multiple different packages which have targets with the same name, please note that "
                "this is currently not supported by Corrosion. Feel free to open an issue on Github to request "
                "supporting this scenario."
                )
            # Skip this target to prevent a hard error.
            continue()
        endif()

        if("staticlib" IN_LIST kinds OR "cdylib" IN_LIST kinds)
            # Explicitly set library names have always been forbidden from using dashes (by cargo).
            # Starting with Rust 1.79, names inherited from the package name will have dashes replaced
            # by underscores too. Corrosion will thus replace dashes with underscores, to make the target
            # name consistent independent of the Rust version. `bin` target names are not affected.
            # See https://github.com/corrosion-rs/corrosion/issues/501 for more details.
            string(REPLACE "\-" "_" target_name "${target_name}")

            set(archive_byproducts "")
            set(shared_lib_byproduct "")
            set(pdb_byproduct "")

            add_library(${target_name} INTERFACE)
            _corrosion_initialize_properties(${target_name})
            _corrosion_add_library_target(
                WORKSPACE_MANIFEST_PATH "${workspace_manifest_path}"
                TARGET_NAME "${target_name}"
                LIB_KINDS ${kinds}
                OUT_ARCHIVE_OUTPUT_BYPRODUCTS archive_byproducts
                OUT_SHARED_LIB_BYPRODUCTS shared_lib_byproduct
                OUT_PDB_BYPRODUCT pdb_byproduct
            )

            set(byproducts "")
            list(APPEND byproducts "${archive_byproducts}" "${shared_lib_byproduct}" "${pdb_byproduct}")

            set(cargo_build_out_dir "")
            _add_cargo_build(
                cargo_build_out_dir
                PACKAGE ${package_name}
                TARGET ${target_name}
                MANIFEST_PATH "${manifest_path}"
                WORKSPACE_MANIFEST_PATH "${workspace_manifest_path}"
                TARGET_KINDS "${kinds}"
                BYPRODUCTS "${byproducts}"
                # Optional
                ${no_linker_override}
            )
            if(archive_byproducts)
                _corrosion_copy_byproducts(
                    ${target_name} ARCHIVE_OUTPUT_DIRECTORY "${cargo_build_out_dir}" "${archive_byproducts}"
                )
            endif()
            if(shared_lib_byproduct)
                _corrosion_copy_byproducts(
                    ${target_name} LIBRARY_OUTPUT_DIRECTORY "${cargo_build_out_dir}" "${shared_lib_byproduct}"
                )
            endif()
            if(pdb_byproduct)
                _corrosion_copy_byproducts(
                    ${target_name} "PDB_OUTPUT_DIRECTORY;LIBRARY_OUTPUT_DIRECTORY" "${cargo_build_out_dir}" "${pdb_byproduct}"
                )
            endif()
            list(APPEND corrosion_targets ${target_name})
            set_property(TARGET "${target_name}" PROPERTY COR_CARGO_PACKAGE_NAME "${package_name}" )
        # Note: "bin" is mutually exclusive with "staticlib/cdylib", since `bin`s are seperate crates from libraries.
        elseif("bin" IN_LIST kinds)
            set(bin_byproduct "")
            set(pdb_byproduct "")
            add_executable(${target_name} IMPORTED GLOBAL)
            _corrosion_initialize_properties(${target_name})
            _corrosion_add_bin_target("${workspace_manifest_path}" "${target_name}"
                "bin_byproduct" "pdb_byproduct"
            )

            set(byproducts "")
            list(APPEND byproducts "${bin_byproduct}" "${pdb_byproduct}")

            set(cargo_build_out_dir "")
            _add_cargo_build(
                cargo_build_out_dir
                PACKAGE "${package_name}"
                TARGET "${target_name}"
                MANIFEST_PATH "${manifest_path}"
                WORKSPACE_MANIFEST_PATH "${workspace_manifest_path}"
                TARGET_KINDS "bin"
                BYPRODUCTS "${byproducts}"
                # Optional
                ${no_linker_override}
            )
            _corrosion_copy_byproducts(
                    ${target_name} RUNTIME_OUTPUT_DIRECTORY "${cargo_build_out_dir}" "${bin_byproduct}"
            )
            if(pdb_byproduct)
                _corrosion_copy_byproducts(
                        ${target_name} "PDB_OUTPUT_DIRECTORY;RUNTIME_OUTPUT_DIRECTORY" "${cargo_build_out_dir}" "${pdb_byproduct}"
                )
            endif()
            list(APPEND corrosion_targets ${target_name})
            set_property(TARGET "${target_name}" PROPERTY COR_CARGO_PACKAGE_NAME "${package_name}" )
        else()
            # ignore other kinds (like examples, tests, build scripts, ...)
        endif()
    endforeach()

    if(NOT corrosion_targets)
        message(DEBUG "No relevant targets found in package ${package_name} - Ignoring")
    else()
        set_target_properties(${corrosion_targets} PROPERTIES INTERFACE_COR_PACKAGE_MANIFEST_PATH "${package_manifest_path}")
    endif()
    set(${out_created_targets} "${corrosion_targets}" PARENT_SCOPE)

endfunction()

# Add all cargo targets defined in the packages defined in the Cargo.toml manifest at
# `MANIFEST_PATH`.
function(_generator_add_cargo_targets)
    set(options NO_LINKER_OVERRIDE)
    set(one_value_args MANIFEST_PATH IMPORTED_CRATES)
    set(multi_value_args CRATES CRATE_TYPES OVERRIDE_CRATE_TYPE_ARGS)
    cmake_parse_arguments(
        GGC
        "${options}"
        "${one_value_args}"
        "${multi_value_args}"
        ${ARGN}
    )
    list(APPEND CMAKE_MESSAGE_CONTEXT "_add_cargo_targets")

    _corrosion_option_passthrough_helper(NO_LINKER_OVERRIDE GGC no_linker_override)
    _corrosion_arg_passthrough_helper(CRATE_TYPES GGC crate_types)
    _corrosion_arg_passthrough_helper(OVERRIDE_CRATE_TYPE_ARGS GGC override_crate_types)

    _cargo_metadata(json "${GGC_MANIFEST_PATH}")
    string(JSON packages GET "${json}" "packages")
    string(JSON workspace_members GET "${json}" "workspace_members")

    string(JSON pkgs_len LENGTH "${packages}")
    math(EXPR pkgs_len-1 "${pkgs_len} - 1")

    string(JSON ws_mems_len LENGTH ${workspace_members})
    math(EXPR ws_mems_len-1 "${ws_mems_len} - 1")

    set(created_targets "")
    set(available_package_names "")
    foreach(ix RANGE ${pkgs_len-1})
        string(JSON pkg GET "${packages}" ${ix})
        string(JSON pkg_id GET "${pkg}" "id")
        string(JSON pkg_name GET "${pkg}" "name")
        string(JSON pkg_manifest_path GET "${pkg}" "manifest_path")
        string(JSON pkg_version GET "${pkg}" "version")
        list(APPEND available_package_names "${pkg_name}")

        if(DEFINED GGC_CRATES)
            if(NOT pkg_name IN_LIST GGC_CRATES)
                continue()
            endif()
        endif()

        # probably this loop is not necessary at all, since when using --no-deps, the
        # contents of packages should already be only workspace members!
        unset(pkg_is_ws_member)
        foreach(ix RANGE ${ws_mems_len-1})
            string(JSON ws_mem GET "${workspace_members}" ${ix})
            if(ws_mem STREQUAL pkg_id)
                set(pkg_is_ws_member YES)
                break()
            endif()
        endforeach()

        if(NOT DEFINED pkg_is_ws_member)
            # Since we pass `--no-deps` to cargo metadata now,  I think this situation can't happen, but lets check for
            # it anyway, just to discover any potential issues.
            # If nobody complains for a while, it should be safe to remove this check and the previous loop, which
            # should speed up the configuration process.
            message(WARNING "The package `${pkg_name}` unexpectedly is not part of the workspace."
                "Please open an issue at corrosion with some background information on the package"
            )
        endif()

        string(JSON targets GET "${pkg}" "targets")

        _generator_add_package_targets(
            WORKSPACE_MANIFEST_PATH "${GGC_MANIFEST_PATH}"
            PACKAGE_MANIFEST_PATH "${pkg_manifest_path}"
            PACKAGE_NAME "${pkg_name}"
            PACKAGE_VERSION "${pkg_version}"
            TARGETS_JSON "${targets}"
            OUT_CREATED_TARGETS curr_created_targets
            ${no_linker_override}
            ${crate_types}
            ${override_crate_types}
        )
        list(APPEND created_targets "${curr_created_targets}")
    endforeach()

    if(NOT created_targets)
        set(crates_error_message "")
        if(DEFINED GGC_CRATES)
            set(crates_error_message "\n`corrosion_import_crate()` was called with the `CRATES` "
                "parameter set to `${GGC_CRATES}`. Corrosion will only attempt to import packages matching "
                    "names from this list."
            )
        endif()
        message(FATAL_ERROR
                "Found no targets in ${pkgs_len} packages."
                ${crates_error_message}.
                "\nPlease keep in mind that corrosion will only import Rust `bin` targets or"
                "`staticlib` or `cdylib` library targets."
                "The following packages were found in the Manifest: ${available_package_names}"
        )
    else()
        message(DEBUG "Corrosion created the following CMake targets: ${created_targets}")
    endif()

    if(GGC_IMPORTED_CRATES)
        set(${GGC_IMPORTED_CRATES} "${created_targets}" PARENT_SCOPE)
    endif()
endfunction()
