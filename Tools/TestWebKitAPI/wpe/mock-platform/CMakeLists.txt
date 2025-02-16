set_property(DIRECTORY . PROPERTY FOLDER "mock-platform")

add_library(WPEMockPlatform MODULE
    WPEDisplayMock.cpp
)

set_target_properties(WPEMockPlatform PROPERTIES
    C_VISIBILITY_PRESET hidden
    POSITION_INDEPENDENT_CODE TRUE
    VISIBILITY_INLINES_HIDDEN TRUE
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/wpe-mock-platform"
)

target_include_directories(WPEMockPlatform PRIVATE include
    ${CMAKE_BINARY_DIR}
    ${WPEPlatform_DERIVED_SOURCES_DIR}
    ${WPEPlatform_DERIVED_SOURCES_DIR}/wpe
    ${WEBKIT_DIR}/WPEPlatform
    ${WEBKIT_DIR}/WPEPlatform/wpe
    ${WTF_FRAMEWORK_HEADERS_DIR}
    ${GLIB_INCLUDE_DIRS}
)

target_link_libraries(WPEMockPlatform
    PRIVATE
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${GLIB_GMODULE_LIBRARIES}
    ${GLIB_LIBRARIES}
    WPEPlatform-${WPE_API_VERSION}
)


WEBKIT_ADD_TARGET_C_FLAGS(WPEMockPlatform
    -Wno-unused-parameter
)

add_library(WPE::MockPlatform ALIAS WPEMockPlatform)
