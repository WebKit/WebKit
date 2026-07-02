list(APPEND bmalloc_PUBLIC_HEADERS
    Configurations/module.modulemap
)

list(APPEND bmalloc_PRIVATE_DEFINITIONS PAS_BMALLOC_HIDDEN=1)

list(APPEND bmalloc_SOURCES
    bmalloc/ProcessCheck.mm
)

find_library(FOUNDATION_LIBRARY Foundation)
list(APPEND bmalloc_LIBRARIES ${FOUNDATION_LIBRARY})

target_compile_options(bmalloc PRIVATE -fno-threadsafe-statics)
