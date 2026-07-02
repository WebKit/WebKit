# FIXME: re-enable for WPE/GTK once forwarded headers land. https://bugs.webkit.org/show_bug.cgi?id=180063
if (COMPILER_IS_CLANG AND NOT COMPILER_IS_CLANG_CL AND NOT PORT STREQUAL "WPE" AND NOT PORT STREQUAL "GTK")
    set(_USE_HEADER_MAPS_DEFAULT ON)
else ()
    set(_USE_HEADER_MAPS_DEFAULT OFF)
endif ()
option(USE_HEADER_MAPS "Collapse per-target include directories into a Clang header map" ${_USE_HEADER_MAPS_DEFAULT})

function(WEBKIT_WRITE_HEADER_MAP target)
    set(options QUOTED BRACKETED)
    set(oneValueArgs DESTINATION)
    set(multiValueArgs FILES)
    cmake_parse_arguments(opt "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    string(REPLACE ";" "\n" body "${opt_FILES}")

    # Only regenerate headermaps if the manifest file has changed. Bump the
    # version number in the path when this function changes and needs to
    # invalidate existing listings.
    set(manifest "${opt_DESTINATION}.v1.txt")
    if (EXISTS ${manifest} AND EXISTS ${opt_DESTINATION})
        file(READ ${manifest} old_body)
        if ("${old_body}" STREQUAL "${body}")
            return()
        endif ()
    endif ()
    file(WRITE ${manifest} ${body})

    # Turn the source lists into a JSON object using a python snippet (CMake's
    # own JSON operations are too slow for thousands of headers). Feed that
    # object into a vendored copy of LLVM hmaptool.
    if (opt_QUOTED AND opt_BRACKETED)
        set(python_code "name, f'${target}/{name}'")
    elseif (opt_QUOTED)
        set(python_code "name")
    elseif (opt_BRACKETED)
        set(python_code "f'${target}/{name}'")
    else ()
        message(AUTHOR_WARNING "Must call with QUOTED and/or BRACKETED argument")
        return()
    endif ()

    execute_process(
        COMMAND ${PYTHON_EXECUTABLE} -c "
import json, os, sys
map = {}
for line in sys.stdin:
    header = line.rstrip()
    name = os.path.basename(header)
    dest = os.path.join('${CMAKE_CURRENT_SOURCE_DIR}', header)
    for entry in [${python_code}]:
        map[entry] = dest
json.dump({'mappings': map}, sys.stdout)
        "
        OUTPUT_FILE ${opt_DESTINATION}.json
        INPUT_FILE ${manifest}
        RESULT_VARIABLE result
    )
    if (result EQUAL 0)
        execute_process(
            COMMAND ${PYTHON_EXECUTABLE} ${TOOLS_DIR}/Scripts/hmaptool write ${opt_DESTINATION}.json ${opt_DESTINATION}
            RESULT_VARIABLE result
        )
    endif ()
    if (NOT result EQUAL 0)
        file(REMOVE ${manifest})
        message(FATAL_ERROR "Generating headermap \"${opt_DESTINATION}\" for ${target} failed")
    endif ()
endfunction()
