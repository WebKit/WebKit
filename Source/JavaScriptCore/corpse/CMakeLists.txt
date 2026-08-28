set(JavaScriptCoreTools_LIBRARY_TYPE STATIC)

set(JavaScriptCoreTools_SOURCES
    CorpseByteParser.cpp
    CorpseClient.cpp
    CorpseError.cpp
    CorpseExportsTrie.cpp
    CorpseProcess.cpp
    CorpseRegion.cpp
    CorpseSnapshot.cpp
    CorpseSymbol.cpp
    CorpseThread.cpp
)

set(JavaScriptCoreTools_PRIVATE_INCLUDE_DIRECTORIES
    $<TARGET_PROPERTY:JavaScriptCore,INCLUDE_DIRECTORIES>
)

set(JavaScriptCoreTools_FRAMEWORKS
    JavaScriptCore
    WTF
    bmalloc
)

WEBKIT_LIBRARY_DECLARE(JavaScriptCoreTools)

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

WEBKIT_LIBRARY(JavaScriptCoreTools)

# The corpse sources use JavaScriptCore's generated headers.
add_dependencies(JavaScriptCoreTools JavaScriptCore)
