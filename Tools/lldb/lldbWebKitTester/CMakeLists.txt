set(lldbWebKitTester_SOURCES
    DumpClassLayoutTesting.cpp
    main.cpp
)

set(lldbWebKitTester_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}  # cmakeconfig.h
)

# The lldb summary providers and dump_class_layout need WTF's debug info in
# this executable's own image, not in a separate framework, so link WTF and
# bmalloc directly instead of through JavaScriptCore.
set(lldbWebKitTester_FRAMEWORKS
    WTF
    bmalloc
)

set(lldbWebKitTester_PRIVATE_LIBRARIES
    "-framework Cocoa"
    "-framework Security"
)

WEBKIT_EXECUTABLE_DECLARE(lldbWebKitTester)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()
WEBKIT_EXECUTABLE(lldbWebKitTester)

# This test works by putting unnused class layouts into the binary, so it
# always needs -O0, full debug info, and unused warnings silenced.
target_compile_options(lldbWebKitTester PRIVATE
    -O0
    -g
    -Wno-unused-private-field
    -Wno-unused-variable
)
target_compile_definitions(lldbWebKitTester PRIVATE
    $<$<NOT:$<CONFIG:Debug>>:RELEASE_WITHOUT_OPTIMIZATIONS>)
