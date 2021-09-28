list(APPEND TestNetscapePlugIn_SOURCES
    PluginObjectMac.mm
)

list(APPEND TestNetscapePlugIn_PRIVATE_INCLUDE_DIRECTORIES
    ${bmalloc_FRAMEWORK_HEADERS_DIR}
    ${WTF_FRAMEWORK_HEADERS_DIR}
)

find_library(COREFOUNDATION_LIBRARY CoreFoundation)
find_library(FOUNDATION_LIBRARY Foundation)
find_library(QUARTZCORE_LIBRARY QuartzCore)
list(APPEND TestNetscapePlugIn_LIBRARIES
    ${COREFOUNDATION_LIBRARY}
    ${FOUNDATION_LIBRARY}
    ${QUARTZCORE_LIBRARY}
)
