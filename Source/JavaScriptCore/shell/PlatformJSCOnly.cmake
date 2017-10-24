# FIXME: https://bugs.webkit.org/show_bug.cgi?id=178730
# Make the Windows build and other builds work more similarly
# rather than having a very different build process with
# library/launcher on Windows and build directly into
# executable elsewhere
if (WIN32)
    include(PlatformWin.cmake)
    add_definitions(-DWIN_CAIRO)
endif ()
