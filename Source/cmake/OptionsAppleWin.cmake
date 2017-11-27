# The static runtime is required for AppleWin due to WebKitSystemInterface.lib
# being compiled with a static runtime.
set(MSVC_STATIC_RUNTIME ON)

include(OptionsWin)

SET_AND_EXPOSE_TO_BUILD(USE_CF ON)
SET_AND_EXPOSE_TO_BUILD(USE_CFURLCONNECTION ON)

set(USE_CA 1)
set(USE_ICU_UNICODE 1)

# Libraries where find_package does not work
set(COREFOUNDATION_LIBRARY CoreFoundation${DEBUG_SUFFIX})
set(LIBXML2_LIBRARIES libxml2${DEBUG_SUFFIX})
set(LIBXSLT_LIBRARIES libxslt${DEBUG_SUFFIX})
set(SQLITE_LIBRARIES SQLite3${DEBUG_SUFFIX})
set(ZLIB_INCLUDE_DIRS "${WEBKIT_LIBRARIES_DIR}/include/zlib")
set(ZLIB_LIBRARIES zdll${DEBUG_SUFFIX})

# Uncomment the following line to try the Direct2D backend.
# set(USE_DIRECT2D 1)

if (${USE_DIRECT2D})
    add_definitions(-DUSE_DIRECT2D=1)
endif ()

# Warnings as errors (ignore narrowing conversions)
add_compile_options(/WX /Wv:18)
