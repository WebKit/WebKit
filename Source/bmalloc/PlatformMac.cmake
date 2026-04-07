add_definitions(-DBPLATFORM_MAC=1)

list(APPEND bmalloc_SOURCES
    bmalloc/ProcessCheck.mm
)

# bmalloc module.modulemap for Swift/module interop (discovered implicitly via -I).
# Same module map as the Xcode build.
list(APPEND bmalloc_PUBLIC_HEADERS
    Configurations/module.modulemap
)
