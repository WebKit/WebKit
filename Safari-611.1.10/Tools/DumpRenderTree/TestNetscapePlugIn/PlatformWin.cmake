list(APPEND TestNetscapePlugIn_SOURCES
    Tests/win/CallJSThatDestroysPlugin.cpp
    Tests/win/DrawsGradient.cpp
    Tests/win/DumpWindowRect.cpp
    Tests/win/GetValueNetscapeWindow.cpp
    Tests/win/NPNInvalidateRectInvalidatesWindow.cpp
    Tests/win/WindowGeometryInitializedBeforeSetWindow.cpp
    Tests/win/WindowRegionIsSetToClipRect.cpp
    Tests/win/WindowlessPaintRectCoordinates.cpp

    win/TestNetscapePlugIn.def
    win/TestNetscapePlugIn.rc
    win/WindowGeometryTest.cpp
    win/WindowedPluginTest.cpp
)

list(APPEND TestNetscapePlugIn_PRIVATE_INCLUDE_DIRECTORIES
    ${TestNetscapePlugIn_DIR}/Tests/win
    ${TestNetscapePlugIn_DIR}/win
)

list(APPEND TestNetscapePlugIn_LIBRARIES
    Msimg32
    Shlwapi
)
