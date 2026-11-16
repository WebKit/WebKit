set_property(DIRECTORY . PROPERTY FOLDER "WTF")

add_subdirectory(wtf)

if (APPLE AND NOT USE_APPLE_INTERNAL_SDK)
    # FIXME: These should be copied by ninja for proper dependency tracking.
    file(COPY ${WTF_DIR}/icu/unicode DESTINATION ${ICU_INCLUDE_DIRS})
    file(COPY ${WTF_DIR}/icu/module.modulemap DESTINATION ${ICU_INCLUDE_DIRS}/unicode)
endif ()
