function(avif_collect_deps target out_deps)
    get_target_property(current_deps ${target} INTERFACE_LINK_LIBRARIES)

    if(NOT current_deps)
        set(current_deps "") # if no dependencies, replace "current_deps-NOTFOUND" with empty list
    endif()

    # Remove entries between ::@(directory-id) and ::@
    # Such entries are added by target_link_libraries() calls outside the target's directory
    set(filtered_deps "")
    set(in_special_block FALSE)
    foreach(dep IN LISTS current_deps)
        if("${dep}" MATCHES "^::@\\(.*\\)$")
            set(in_special_block TRUE) # Latch on
        elseif("${dep}" STREQUAL "::@")
            set(in_special_block FALSE) # Latch off
        elseif(NOT in_special_block)
            string(REGEX REPLACE "\\\$<BUILD_INTERFACE:([^>]*)>" "\\1" dep ${dep})
            string(REGEX REPLACE "\\\$<LINK_ONLY:([^>]*)>" "\\1" dep ${dep})
            if(TARGET ${dep})
                get_target_property(_aliased_dep ${dep} ALIASED_TARGET)
                if(_aliased_dep)
                    list(APPEND filtered_deps ${_aliased_dep})
                else()
                    list(APPEND filtered_deps ${dep})
                endif()
            endif()
        endif()
    endforeach()

    set(all_deps ${filtered_deps})

    foreach(dep IN LISTS filtered_deps)
        # Avoid infinite recursion if the target has a cyclic dependency
        if(NOT "${dep}" IN_LIST ${out_deps})
            avif_collect_deps(${dep} dep_child_deps)
            list(APPEND all_deps ${dep_child_deps})
        endif()
    endforeach()

    list(REMOVE_DUPLICATES all_deps)

    set(${out_deps} "${all_deps}" PARENT_SCOPE)
endfunction()

function(merge_static_libs target in_target)
    set(args ${ARGN})

    set(dependencies ${in_target})
    set(libs $<TARGET_FILE:${in_target}>)

    set(source_file ${CMAKE_CURRENT_BINARY_DIR}/${target}_depends.c)
    add_library(${target} STATIC ${source_file})

    avif_collect_deps(${in_target} lib_deps)

    foreach(lib ${lib_deps})
        get_target_property(child_target_type ${lib} TYPE)
        if(${child_target_type} STREQUAL "INTERFACE_LIBRARY")
            continue()
        endif()
        get_target_property(link_dirs ${lib} INTERFACE_LINK_DIRECTORIES)
        if(link_dirs)
            target_link_directories(${target} PUBLIC ${link_dirs})
        endif()
        get_target_property(is_avif_local ${lib} AVIF_LOCAL)
        if(is_avif_local)
            list(APPEND libs $<TARGET_FILE:${lib}>)
            list(APPEND dependencies "${lib}")
        endif()
    endforeach()

    list(REMOVE_DUPLICATES libs)

    add_custom_command(
        OUTPUT ${source_file} DEPENDS ${dependencies} COMMAND ${CMAKE_COMMAND} -E echo \"const int dummy = 0\;\" > ${source_file}
    )

    add_custom_command(TARGET ${target} POST_BUILD COMMAND ${CMAKE_COMMAND} -E remove $<TARGET_FILE:${target}>)

    if(APPLE)
        add_custom_command(
            TARGET ${target}
            POST_BUILD
            COMMENT "Merge static libraries with libtool"
            COMMAND xcrun libtool -static -o $<TARGET_FILE:${target}> -no_warning_for_no_symbols ${libs}
        )
    elseif(CMAKE_C_COMPILER_ID MATCHES "^(Clang|GNU|Intel|IntelLLVM)$")
        add_custom_command(
            TARGET ${target}
            POST_BUILD
            COMMENT "Merge static libraries with ar"
            COMMAND ${CMAKE_COMMAND} -E echo CREATE $<TARGET_FILE:${target}> >script.ar
        )

        foreach(lib ${libs})
            add_custom_command(TARGET ${target} POST_BUILD COMMAND ${CMAKE_COMMAND} -E echo ADDLIB ${lib} >>script.ar)
        endforeach()

        add_custom_command(
            TARGET ${target}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo SAVE >>script.ar
            COMMAND ${CMAKE_COMMAND} -E echo END >>script.ar
            COMMAND ${CMAKE_AR} -M <script.ar
            COMMAND ${CMAKE_COMMAND} -E remove script.ar
        )
    elseif(MSVC)
        if(CMAKE_LIBTOOL)
            set(BUNDLE_TOOL ${CMAKE_LIBTOOL})
        else()
            find_program(BUNDLE_TOOL lib HINTS "${CMAKE_C_COMPILER}/..")

            if(NOT BUNDLE_TOOL)
                message(FATAL_ERROR "Cannot locate lib.exe to bundle libraries")
            endif()
        endif()

        add_custom_command(
            TARGET ${target}
            POST_BUILD
            COMMENT "Merge static libraries with lib.exe"
            COMMAND ${BUNDLE_TOOL} /NOLOGO /OUT:$<TARGET_FILE:${target}> ${libs}
        )
    else()
        message(FATAL_ERROR "Unsupported platform for static link merging")
    endif()
endfunction()
