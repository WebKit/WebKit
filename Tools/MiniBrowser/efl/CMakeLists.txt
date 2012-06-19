SET(MiniBrowser_DIR "${TOOLS_DIR}/MiniBrowser/efl")

SET(MiniBrowser_SOURCES
    ${MiniBrowser_DIR}/main.c
)

SET(MiniBrowser_INCLUDE_DIRECTORIES
    ${CAIRO_INCLUDE_DIRS}
    ${ECORE_X_INCLUDE_DIRS}
    ${EDJE_INCLUDE_DIRS}
    ${EFLDEPS_INCLUDE_DIRS}
    ${EVAS_INCLUDE_DIRS}
    ${WEBKIT2_DIR}/UIProcess/API/efl
    ${WEBKIT2_DIR}
    ${DERIVED_SOURCES_WEBKIT2_DIR}/include
    ${CMAKE_SOURCE_DIR}/Source
)

SET(MiniBrowser_LIBRARIES
    ${JavaScriptCore_LIBRARY_NAME}
    ${WebCore_LIBRARY_NAME}
    ${WebKit2_LIBRARY_NAME}
    ${CAIRO_LIBRARIES}
    ${ECORE_X_LIBRARIES}
    ${EDJE_LIBRARIES}
    ${EFLDEPS_LIBRARIES}
    ${EVAS_LIBRARIES}
    ${FONTCONFIG_LIBRARIES}
    ${LIBSOUP24_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    ${LIBXSLT_LIBRARIES}
    ${SQLITE_LIBRARIES}
)

SET(MiniBrowser_LINK_FLAGS
    ${ECORE_X_LDFLAGS}
    ${EDJE_LDFLAGS}
    ${EFLDEPS_LDFLAGS}
    ${EVAS_LDFLAGS}
    ${LIBSOUP24_LDFLAGS}
)

IF (ENABLE_GLIB_SUPPORT)
    LIST(APPEND MiniBrowser_LIBRARIES
        ${Glib_LIBRARIES}
        ${Gthread_LIBRARIES}
    )
ENDIF ()

ADD_CUSTOM_TARGET(forwarding-headerMiniBrowserEfl
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl ${MiniBrowser_DIR} ${DERIVED_SOURCES_WEBKIT2_DIR}/include efl
)

ADD_CUSTOM_TARGET(forwarding-headerMiniBrowserSoup
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl ${MiniBrowser_DIR} ${DERIVED_SOURCES_WEBKIT2_DIR}/include soup
)

INCLUDE_DIRECTORIES(${MiniBrowser_INCLUDE_DIRECTORIES})

ADD_EXECUTABLE(MiniBrowser ${MiniBrowser_SOURCES})
TARGET_LINK_LIBRARIES(MiniBrowser ${MiniBrowser_LIBRARIES})
ADD_TARGET_PROPERTIES(MiniBrowser LINK_FLAGS "${MiniBrowser_LINK_FLAGS}")
SET_TARGET_PROPERTIES(MiniBrowser PROPERTIES FOLDER "Tools")

ADD_DEPENDENCIES(MiniBrowser forwarding-headerMiniBrowserEfl)
ADD_DEPENDENCIES(MiniBrowser forwarding-headerMiniBrowserSoup)
