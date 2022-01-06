if (${CMAKE_GENERATOR} MATCHES "Visual Studio")
    # With the VisualStudio generator, the compiler complains about -std=c++* for C sources.
    set_source_files_properties(
        ${bmalloc_SOURCES}
        PROPERTIES LANGUAGE CXX
    )
endif ()

