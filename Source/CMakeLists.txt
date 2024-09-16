# -----------------------------------------------------------------------------
# Add module directories
# -----------------------------------------------------------------------------
if (NOT USE_SYSTEM_MALLOC)
    add_subdirectory(bmalloc)
endif ()

add_subdirectory(WTF)

if (USE_CAPSTONE)
    add_subdirectory(ThirdParty/capstone)
endif ()

if (USE_SYSTEM_UNIFDEF)
  find_package(Unifdef REQUIRED)
else ()
  add_subdirectory(ThirdParty/unifdef)
endif ()

add_subdirectory(JavaScriptCore)

if (ENABLE_WEBGL OR USE_ANGLE_EGL)
    add_subdirectory(ThirdParty/ANGLE)
endif ()

if (ENABLE_API_TESTS)
    add_subdirectory(ThirdParty/gtest)
endif ()

if (USE_XDGMIME)
    add_subdirectory(ThirdParty/xdgmime)
endif ()

if (USE_LIBWEBRTC)
    add_subdirectory(ThirdParty/libwebrtc)
    include_directories(${CMAKE_CURRENT_BINARY_DIR}/ThirdParty/libwebrtc)
endif ()

if (USE_SKIA)
    add_subdirectory(ThirdParty/skia)
endif ()

if (USE_SYSTEM_SYSPROF_CAPTURE)
    find_package(SysProfCapture)
    if (NOT SysProfCapture_FOUND)
        message(FATAL_ERROR "system libsysprof-capture-4 not found, "
                            "consider using USE_SYSTEM_SYSPROF_CAPTURE=NO")
    endif ()
elseif (USE_SYSPROF_CAPTURE)
    add_subdirectory(ThirdParty/libsysprof-capture)
endif ()

if (ENABLE_WEBINSPECTORUI)
    add_subdirectory(WebInspectorUI)
endif ()

if (ENABLE_WEBCORE)
    add_subdirectory(WebCore)
endif ()

if (ENABLE_WEBKIT_LEGACY)
    add_subdirectory(WebKitLegacy)
endif ()

if (ENABLE_WEBKIT)
    add_subdirectory(WebKit)
endif ()

if (ENABLE_WEBDRIVER)
    add_subdirectory(WebDriver)
endif ()

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()
