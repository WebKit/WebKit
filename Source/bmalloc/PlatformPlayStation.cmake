WEBKIT_APPEND_GLOBAL_COMPILER_FLAGS(
    -Wno-typedef-redefinition)

if (${CMAKE_GENERATOR} MATCHES "Visual Studio")
    set(bmalloc_C_SOURCES ${bmalloc_SOURCES})
    list(FILTER bmalloc_C_SOURCES INCLUDE REGEX "\\.c$")

    # With the VisualStudio generator, the compiler complains about -std=c++* for C sources.
    set_source_files_properties(
        ${bmalloc_C_SOURCES}
        PROPERTIES LANGUAGE C
        COMPILE_OPTIONS --std=gnu17
    )
endif ()

