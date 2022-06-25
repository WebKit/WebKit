INCLUDE_DIRECTORIES(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${yasm_SOURCE_DIR}/frontends/yasm
    )

ADD_CUSTOM_COMMAND(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/license.c
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/frontends/yasm/genstring.py
        license_msg
        ${CMAKE_CURRENT_BINARY_DIR}/license.c
        ${CMAKE_SOURCE_DIR}/COPYING
    MAIN_DEPENDENCY ${CMAKE_SOURCE_DIR}/COPYING
    DEPENDS ${CMAKE_SOURCE_DIR}/frontends/yasm/genstring.py
    )

SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

IF(BUILD_SHARED_LIBS)
    ADD_EXECUTABLE(vsyasm
        vsyasm.c
        ${yasm_SOURCE_DIR}/frontends/yasm/yasm-options.c
        ${yasm_SOURCE_DIR}/frontends/yasm/yasm-plugin.c
        )
    TARGET_LINK_LIBRARIES(vsyasm libyasm ${LIBDL})
ELSE(BUILD_SHARED_LIBS)
    ADD_EXECUTABLE(vsyasm
        vsyasm.c
        ${yasm_SOURCE_DIR}/frontends/yasm/yasm-options.c
        )
    TARGET_LINK_LIBRARIES(vsyasm yasmstd libyasm)
ENDIF(BUILD_SHARED_LIBS)

SET_SOURCE_FILES_PROPERTIES(vsyasm.c PROPERTIES
    OBJECT_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/license.c
    )

INSTALL(TARGETS vsyasm RUNTIME DESTINATION bin)
