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
        CoreFoundation
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
