set_property(DIRECTORY . PROPERTY FOLDER "WTF")

add_subdirectory(wtf)

if (APPLE AND NOT USE_APPLE_INTERNAL_SDK)
    file(COPY ${WTF_DIR}/icu/unicode DESTINATION ${ICU_INCLUDE_DIRS})
endif ()
