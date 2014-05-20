include(FindGettext)

# GETTEXT_CREATE_TRANSLATIONS automatically runs msgmerge, which is something we
# didn't do in the old autotools build. This overwrites all the po files in the
# Source directory. Perhaps we want this, but for now disable it to maintain compatibility.
set(GETTEXT_MSGMERGE_EXECUTABLE :)

set(domain WebKit2GTK-3.0)
set(build_directory ${CMAKE_BINARY_DIR}/Source/WebCore/platform/gtk/po)
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
GETTEXT_CREATE_TRANSLATIONS(${pot_file} ALL ${po_files})
