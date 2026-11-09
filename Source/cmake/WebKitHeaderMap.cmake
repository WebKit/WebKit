# FIXME: re-enable for WPE/GTK once forwarded headers land. https://bugs.webkit.org/show_bug.cgi?id=180063
if (COMPILER_IS_CLANG AND NOT COMPILER_IS_CLANG_CL AND NOT PORT STREQUAL "WPE" AND NOT PORT STREQUAL "GTK")
    set(_USE_HEADER_MAPS_DEFAULT ON)
else ()
    set(_USE_HEADER_MAPS_DEFAULT OFF)
endif ()
option(USE_HEADER_MAPS "Collapse per-target include directories into a Clang header map" ${_USE_HEADER_MAPS_DEFAULT})

# Header maps are written at the end of the configure, all in one Python
# process (Tools/Scripts/generate-header-maps). Spawning Python twice per header
# map, as this used to, was most of the time spent configuring a new build
# directory. Nothing reads a header map before the build starts.
function(WEBKIT_WRITE_HEADER_MAP target)
    set(options QUOTED BRACKETED)
    set(oneValueArgs DESTINATION)
    set(multiValueArgs FILES)
    cmake_parse_arguments(opt "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT opt_QUOTED AND NOT opt_BRACKETED)
        message(AUTHOR_WARNING "Must call with QUOTED and/or BRACKETED argument")
        return ()
    endif ()

    set(quoted 0)
    set(bracketed 0)
    if (opt_QUOTED)
        set(quoted 1)
    endif ()
    if (opt_BRACKETED)
        set(bracketed 1)
    endif ()
    string(REPLACE ";" "\n" files "${opt_FILES}")

    # The manifest lists what the header map is made from. file(CONFIGURE) leaves
    # it alone when the listing is unchanged, so the header map only needs writing
    # when it is missing or not newer than the manifest. generate-header-maps
    # writes the header map after the manifest, so a configure that fails before
    # then retries next time. Bump the version number in the suffix (here and in
    # generate-header-maps) when this function changes and needs to invalidate
    # existing listings.
    set(manifest "${opt_DESTINATION}.v2.txt")
    file(CONFIGURE OUTPUT "${manifest}" CONTENT "${target}|${CMAKE_CURRENT_SOURCE_DIR}|${quoted}|${bracketed}\n${files}" @ONLY)
    # IS_NEWER_THAN is also true for equal timestamps and missing files.
    if (NOT "${manifest}" IS_NEWER_THAN "${opt_DESTINATION}")
        return ()
    endif ()
    set_property(GLOBAL APPEND PROPERTY WEBKIT_PENDING_HEADER_MAPS "${opt_DESTINATION}")
endfunction()

function(_WEBKIT_WRITE_PENDING_HEADER_MAPS)
    get_property(destinations GLOBAL PROPERTY WEBKIT_PENDING_HEADER_MAPS)
    if (NOT destinations)
        return ()
    endif ()
    set_property(GLOBAL PROPERTY WEBKIT_PENDING_HEADER_MAPS "")
    list(REMOVE_DUPLICATES destinations)
    list(LENGTH destinations count)
    string(REPLACE ";" "\n" jobs "${destinations}")
    set(jobs_file "${CMAKE_BINARY_DIR}/CMakeFiles/pending-header-maps.txt")
    file(WRITE ${jobs_file} "${jobs}\n")
    execute_process(
        COMMAND ${PYTHON_EXECUTABLE} ${TOOLS_DIR}/Scripts/generate-header-maps ${jobs_file}
        RESULT_VARIABLE result
    )
    if (NOT result EQUAL 0)
        message(FATAL_ERROR "Generating ${count} header maps failed")
    endif ()
    message(STATUS "Generated ${count} header maps")
endfunction()

# Runs once every directory has been processed.
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL _WEBKIT_WRITE_PENDING_HEADER_MAPS)
