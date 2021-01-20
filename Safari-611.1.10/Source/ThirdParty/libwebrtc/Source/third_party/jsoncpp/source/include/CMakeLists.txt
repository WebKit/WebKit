file(GLOB INCLUDE_FILES "json/*.h")
install(FILES
    ${INCLUDE_FILES}
    ${PROJECT_BINARY_DIR}/include/json/version.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/json)

