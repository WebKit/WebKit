add_definitions(-DBPLATFORM_MAC=1)

list(APPEND bmalloc_PUBLIC_HEADERS
    Configurations/module.modulemap
)

list(APPEND bmalloc_SOURCES
    bmalloc/ProcessCheck.mm
)

# ProcessCheck.mm uses Foundation (NSBundle, NSProcessInfo).
# bmalloc is an OBJECT library so framework deps must be explicit.
find_library(FOUNDATION_LIBRARY Foundation)
list(APPEND bmalloc_LIBRARIES ${FOUNDATION_LIBRARY})
