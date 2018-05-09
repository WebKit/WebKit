find_package(LibGBM REQUIRED)
find_package(WPEBackend-fdo REQUIRED)

set(WPEHeadlessViewBackend_SOURCES
    ${TOOLS_DIR}/wpe/HeadlessViewBackend/HeadlessViewBackend.cpp
)

set(WPEHeadlessViewBackend_SYSTEM_INCLUDE_DIRECTORIES
    ${CAIRO_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${LIBGBM_INCLUDE_DIRS}
    ${WPEBACKEND_FDO_INCLUDE_DIRS}
)

set(WPEHeadlessViewBackend_LIBRARIES
    ${CAIRO_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${LIBGBM_LIBRARIES}
    ${WPEBACKEND_FDO_LIBRARIES}
)

add_library(WPEHeadlessViewBackend ${WPEHeadlessViewBackend_SOURCES})
include_directories(SYSTEM ${WPEHeadlessViewBackend_SYSTEM_INCLUDE_DIRECTORIES})
target_link_libraries(WPEHeadlessViewBackend ${WPEHeadlessViewBackend_LIBRARIES})
