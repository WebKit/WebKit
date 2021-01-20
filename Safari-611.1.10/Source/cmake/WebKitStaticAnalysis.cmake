
option(ANALYZERS "Static analysis tools to run on the build" "")

if ("clang-tidy" IN_LIST ANALYZERS)
    find_program(ClangTidy_EXE
        NAMES clang-tidy
        PATHS
            "C:/Program Files/LLVM/bin"
            "C:/Program Files (x86)/LLVM/bin"
    )
    if (ClangTidy_EXE)
        message(STATUS "Found clang-tidy: ${ClangTidy_EXE}")
    else ()
        message(FATAL_ERROR "Could not find clang-tidy static analyzer")
    endif ()
endif ()

if ("iwyu" IN_LIST ANALYZERS)
    find_program(IWYU_EXE
        NAMES iwyu include-what-you-use
        PATHS
            "C:/Program Files/include-what-you-use/bin"
            "C:/Program Files (x86)/include-what-you-use/bin"
    )
    if (IWYU_EXE)
        message(STATUS "Found include-what-you-use: ${IWYU_EXE}")
    else ()
        message(FAT "Could not find include-what-you-use static analyzer")
    endif ()
endif ()

if ("lwyu" IN_LIST ANALYZERS)
    set(CMAKE_LINK_WHAT_YOU_USE ON)
endif ()
