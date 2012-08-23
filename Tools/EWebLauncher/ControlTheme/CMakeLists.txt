SET(ControlTheme_DIR "${TOOLS_DIR}/EWebLauncher/ControlTheme")

ADD_CUSTOM_TARGET(ControlTheme ALL
    COMMAND ${EDJE_CC_EXECUTABLE} -v ${ControlTheme_DIR}/entry.edc ${THEME_BINARY_DIR}/entry.edj
    DEPENDS
        ${ControlTheme_DIR}/entry.edc
    VERBATIM
)
