list(APPEND JavaScriptCore_SOURCES
    API/JSStringRefBSTR.cpp
    API/JSStringRefCF.cpp
)

if (WTF_PLATFORM_WIN_CAIRO)
    list(APPEND JavaScriptCore_LIBRARIES
        CFLite
    )
else ()
    list(APPEND JavaScriptCore_LIBRARIES
        CoreFoundation${DEBUG_SUFFIX}
        ${ICU_LIBRARIES}
    )
endif ()

if (MSVC AND "${JavaScriptCore_LIBRARY_TYPE}" MATCHES "SHARED")
    get_property(WTF_LIBRARY_LOCATION TARGET WTF PROPERTY LOCATION)

    add_custom_command(
        OUTPUT ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/forwarded-exports.cpp
        DEPENDS WTF
        COMMAND ${PYTHON_EXECUTABLE} ${TOOLS_DIR}/Scripts/generate-win32-export-forwards ${WTF_LIBRARY_LOCATION} ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/forwarded-exports.cpp
        VERBATIM)
    list(APPEND JavaScriptCore_SOURCES ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/forwarded-exports.cpp)
endif ()

list(REMOVE_ITEM JavaScriptCore_SOURCES
    inspector/JSGlobalObjectInspectorController.cpp
)

file(COPY
    "${JAVASCRIPTCORE_DIR}/JavaScriptCore.vcxproj/JavaScriptCore.resources"
    DESTINATION
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

file(MAKE_DIRECTORY ${DERIVED_SOURCES_DIR}/ForwardingHeaders/JavaScriptCore)
set(JavaScriptCore_POST_BUILD_COMMAND "${CMAKE_BINARY_DIR}/DerivedSources/JavaScriptCore/postBuild.cmd")
file(WRITE "${JavaScriptCore_POST_BUILD_COMMAND}" "@xcopy /y /d /f \"${DERIVED_SOURCES_DIR}/JavaScriptCore/*.h\" \"${DERIVED_SOURCES_DIR}/ForwardingHeaders/JavaScriptCore\" >nul 2>nul\n")
file(APPEND "${JavaScriptCore_POST_BUILD_COMMAND}" "@xcopy /y /d /f \"${DERIVED_SOURCES_DIR}/JavaScriptCore/inspector/*.h\" \"${DERIVED_SOURCES_DIR}/ForwardingHeaders/JavaScriptCore\" >nul 2>nul\n")
foreach (_directory ${JavaScriptCore_FORWARDING_HEADERS_DIRECTORIES})
    file(APPEND "${JavaScriptCore_POST_BUILD_COMMAND}" "@xcopy /y /d /f \"${JAVASCRIPTCORE_DIR}/${_directory}/*.h\" \"${DERIVED_SOURCES_DIR}/ForwardingHeaders/JavaScriptCore\" >nul 2>nul\n")
endforeach ()

set(JavaScriptCore_OUTPUT_NAME JavaScriptCore${DEBUG_SUFFIX})
