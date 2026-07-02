include(platform/ImageDecoders.cmake)
include(platform/OpenSSL.cmake)

list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
  "${WEBCORE_DIR}/platform/glib"
)

list(APPEND WebCore_SOURCES
  accessibility/playstation/AXObjectCachePlayStation.cpp
  accessibility/playstation/AccessibilityObjectPlayStation.cpp

  editing/glib/EditorGLib.cpp

  page/PointerLockController.cpp

  page/haiku/DragControllerHaiku.cpp
  page/haiku/EventHandlerHaiku.cpp

  platform/audio/FFTFrameStub.cpp

  platform/graphics/PlatformDisplay.cpp
  platform/graphics/WOFFFileFormat.cpp

  platform/mock/GeolocationClientMock.cpp

  platform/text/Hyphenation.cpp
  platform/text/LocaleICU.cpp

  platform/unix/LoggingUnix.cpp
  platform/unix/SharedMemoryUnix.cpp
)

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WebCore_DERIVED_SOURCES_DIR}/ModernMediaControls.css
)

set(WebCore_USER_AGENT_SCRIPTS
    ${WebCore_DERIVED_SOURCES_DIR}/ModernMediaControls.js
)

list(APPEND WebCore_LIBRARIES
  be
  bsd
  execinfo
  ${JPEG_LIBRARY}
  network
  ${PNG_LIBRARY}
  psl
  shared
  textencoding
  translation
  ${WEBP_LIBRARIES}
  ${LIBXSLT_LIBRARIES}
)

list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
  ${GNUTLS_INCLUDE_DIRS}
  ${ICU_INCLUDE_DIRS}
  ${LIBXML2_INCLUDE_DIR}
  ${LIBXSLT_INCLUDE_DIR}
  ${SQLITE_INCLUDE_DIR}
  ${WEBP_INCLUDE_DIRS}
  ${ZLIB_INCLUDE_DIRS}
)

set(CSS_VALUE_PLATFORM_DEFINES "HAVE_OS_DARK_MODE_SUPPORT=1")
