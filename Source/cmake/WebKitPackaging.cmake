# -----------------------------------------------------------------------------
# This file defines the basics of CPack behavior for WebKit
#
# The following CPack variables will be defined if they were unset:
#   - CPACK_PACKAGE_NAME to WebKit-${PORT}
#   - CPACK_SOURCE_IGNORE_FILES to a known pattern of good files
#
# The following variables affect the behavior of packaging:
#   - WEBKIT_CPACK_ALL_PORTS if defined and true, will not limit packaging
#     to just include files of the port (affects CPACK_SOURCE_IGNORE_FILES,
#     just if this variable was not defined before).
#   - WEBKIT_CPACK_ADD_TESTS if defined and true, will also add tests
#     (affects CPACK_SOURCE_IGNORE_FILES, just if this variable was
#     not defined before)
#   - WEBKIT_CPACK_ADD_TOOLS if defined and true, will also add tools
#     (affects CPACK_SOURCE_IGNORE_FILES, just if this variable was
#     not defined before)
# -----------------------------------------------------------------------------

IF(NOT DEFINED CPACK_PACKAGE_NAME)
  SET(CPACK_PACKAGE_NAME WebKit-${PORT})
ENDIF()

IF(NOT DEFINED CPACK_SOURCE_IGNORE_FILES)
  SET(CPACK_SOURCE_IGNORE_FILES
    # Version control:
    "/CVS/"
    "/\\\\.svn/"
    "/\\\\.bzr/"
    "/\\\\.hg/"
    "/\\\\.git/"
    "\\\\.swp$"
    "\\\\.#"
    "/#"
    "/\\\\.gitignore$"
    "/\\\\.gitattributes$"

    # SVN-only files should be ignored (site, examples...)
    "/PerformanceTests/"
    "/Examples/"
    "/Websites/"

    # Other build systems:
    # - Makefiles (.mk/Makefile)
    "\\\\.mk$"
    "\\\\.make$"
    "Makefile"
    # - Autotools (GTK)
    "/autotools/"
    "/configure\\\\.ac"
    "/autogen\\\\.sh"
    "/autom4te\\\\.cache/"
    "/aclocal\\\\.m4$"
    "/GNUmakefile"
    "/GNUmakefile"
    # - XCode (Mac)
    "\\\\.xcodeproj"
    "\\\\.xcconfig"
    # - GYP
    "\\\\.gyp"
    # - QMake (Qt)
    "\\\\.pri$"
    "\\\\.pro$"

    # Development & Runtime created files
    "~$"
    "\\\\.mode"
    "\\\\.pbxuser$"
    "\\\\.perspective"
    "\\\\.pyc$"
    "\\\\.pyo$"
    "/cmake-build/"
    "/build/"
    "/WebKitBuild/"
    "/Tools/Scripts/webkitpy/thirdparty/autoinstalled/"
    )

  IF(NOT WEBKIT_CPACK_ADD_TESTS)
    LIST(APPEND CPACK_SOURCE_IGNORE_FILES
      "/LayoutTests/"
      "/ManualTests/"
      "/tests/"
      )
  ENDIF(NOT WEBKIT_CPACK_ADD_TESTS)

  IF(NOT WEBKIT_CPACK_ADD_TOOLS)
    LIST(APPEND CPACK_SOURCE_IGNORE_FILES
      "/Tools/"
      "/manual-tools/"
      "/tools/"
      "/PageLoadTools/"
      )
  ENDIF(NOT WEBKIT_CPACK_ADD_TOOLS)


  IF(NOT WEBKIT_CPACK_ALL_PORTS)

    # All file and directory patterns that Efl uses
    SET(FILE_PATTERNS_Efl
      "/cairo/" "/Cairo/" "cairo\\\\." "Cairo\\\\."
      "/efl/" "/Efl/" "efl\\\\." "Efl\\\\."
      "/glib/" "/Glib/" "glib\\\\." "Glib\\\\."
      "/gobject/" "/Gobject/" "gobject\\\\." "Gobject\\\\."
      "/icu/" "/Icu/" "icu\\\\." "Icu\\\\."
      "/posix/" "/Posix/" "posix\\\\." "Posix\\\\."
      "/soup/" "/Soup/" "soup\\\\." "Soup\\\\."
      )

    # File and Directory patterns that no CMake-ified port uses
    SET(FILE_PATTERNS_UNKNOWN_PORTS
      "/carbon/" "/Carbon/" "carbon\\\\." "Carbon\\\\."
      "/cf/" "/Cf/" "cf\\\\." "Cf\\\\."
      "/cg/" "/Cg/" "cg\\\\." "Cg\\\\."
      "/chromium/" "/Chromium/" "chromium\\\\." "Chromium\\\\."
      "/cocoa/" "/Cocoa/" "cocoa\\\\." "Cocoa\\\\."
      "/Configurations/" "/Configurations/" "Configurations\\\\." "Configurations\\\\."
      "/curl/" "/Curl/" "curl\\\\." "Curl\\\\."
      "/gstreamer/" "/Gstreamer/" "gstreamer\\\\." "Gstreamer\\\\."
      "/gtk/" "/Gtk/" "gtk\\\\." "Gtk\\\\."
      "/iphone/" "/Iphone/" "iphone\\\\." "Iphone\\\\."
      "/mac/" "/Mac/" "mac\\\\." "Mac\\\\."
      "/opentype/" "/Opentype/" "opentype\\\\." "Opentype\\\\."
      "/openvg/" "/Openvg/" "openvg\\\\." "Openvg\\\\."
      "/os-win32/" "/Os-Win32/" "os-win32\\\\." "Os-Win32\\\\."
      "/qscriptengine/" "/Qscriptengine/" "qscriptengine\\\\." "Qscriptengine\\\\."
      "/qscriptstring/" "/Qscriptstring/" "qscriptstring\\\\." "Qscriptstring\\\\."
      "/qscriptvalue/" "/Qscriptvalue/" "qscriptvalue\\\\." "Qscriptvalue\\\\."
      "/qt/" "/Qt/" "qt\\\\." "Qt\\\\."
      "/qt4/" "/Qt4/" "qt4\\\\." "Qt4\\\\."
      "/skia/" "/Skia/" "skia\\\\." "Skia\\\\."
      "/symbian/" "/Symbian/" "symbian\\\\." "Symbian\\\\."
      "/v8/" "/V8/" "v8\\\\." "V8\\\\."
      "/V8/" "/V8/" "V8\\\\." "V8\\\\."
      "/win/" "/Win/" "win\\\\." "Win\\\\."
      "/wince/" "/Wince/" "wince\\\\." "Wince\\\\."
      "/wx/" "/Wx/" "wx\\\\." "Wx\\\\."
      "/wxcode/" "/Wxcode/" "wxcode\\\\." "Wxcode\\\\."
      "/WebKitLibraries/"
      "/English\\\\.lproj/"
      "/Source/WebKit2/"
      "\\\\.a$"
      "\\\\.exe$"
      "\\\\.mm$"
      )

    # Append all Unknown port patterns
    FOREACH(_pattern ${FILE_PATTERNS_UNKNOWN_PORTS})
      LIST(FIND FILE_PATTERNS_${PORT} ${_pattern} _pattern_index)
      IF(_pattern_index GREATER -1)
        MESSAGE("pattern ${_pattern} declared of 'no-port' is actually used by ${PORT}")
      ELSE()
        LIST(APPEND CPACK_SOURCE_IGNORE_FILES ${_pattern})
      ENDIF()
    ENDFOREACH()

    # Append all "other-ports" patterns
    FOREACH(_port ${ALL_PORTS})
      IF(NOT ${_port} STREQUAL ${PORT})
        FOREACH(_pattern ${FILE_PATTERNS_${_port}})

          LIST(FIND FILE_PATTERNS_${PORT} ${_pattern} _pattern_index)
          IF(_pattern_index GREATER -1)
            MESSAGE("pattern ${_pattern} of port ${_port} is also used by ${PORT}")
          ELSE()
            LIST(APPEND CPACK_SOURCE_IGNORE_FILES ${_pattern})
          ENDIF()
        ENDFOREACH()
      ENDIF()
    ENDFOREACH()

  ENDIF(NOT WEBKIT_CPACK_ALL_PORTS)

ENDIF(NOT DEFINED CPACK_SOURCE_IGNORE_FILES)

# -----------------------------------------------------------------------------
# Include CPack that will define targets based on the variables defined before
# -----------------------------------------------------------------------------
INCLUDE(CPack)
