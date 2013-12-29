set(domain WebKitGTK-3.0)
set(build_directory ${CMAKE_BINARY_DIR}/po)
set(pot_file ${build_directory}/${domain}.pot)
set(potfiles_file ${build_directory}/POTFILES)

file(MAKE_DIRECTORY ${build_directory})
configure_file(POTFILES.in ${potfiles_file})

add_custom_command(
    OUTPUT ${pot_file}
    DEPENDS ${potfiles_file}
    COMMAND xgettext
            --default-domain=${domain}
            --add-comments=TRANSLATORS:
            --msgid-bugs-address="http://bugs.webkit.org"
            --files-from=${potfiles_file}
            --package-version=${PROJECT_VERSION}
            --package-name="webkitgtk"
            --keyword=_
            --keyword=N_
            -o ${pot_file}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
)

file(GLOB po_files *.po)
foreach (file ${po_files})
    get_filename_component(language ${file} NAME_WE)
    set(mo_file ${build_directory}/${language}.mo)
    list(APPEND mo_files ${mo_file})

    add_custom_command(
        OUTPUT ${mo_file}
        DEPENDS ${file}
        DEPENDS ${pot_file}
        COMMAND msgfmt -c -o ${mo_file} ${file}
    )
endforeach ()

add_custom_target(update-mo-files ALL
                  DEPENDS ${mo_files}
)

add_custom_target(update-po ALL
                  DEPENDS update-mo-files
                  DEPENDS ${pot_file}
)

