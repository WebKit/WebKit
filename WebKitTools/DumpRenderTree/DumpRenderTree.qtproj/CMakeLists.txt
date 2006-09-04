include_directories(
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../../WebCore
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../../JavaScriptCore
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../../WebCore/bridge
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../../WebCore/page
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../../WebCore/loader
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../../WebCore/platform
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../../WebCore/platform/qt
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../../WebCore/ksvg2
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../../WebCore/ksvg2/svg
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../../WebCore/ksvg2/css
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../../WebCore/rendering
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../../WebCore/editing
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../../WebCore/html
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../../WebCore/css
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../../WebCore/dom
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../../WebCore/xml
   BEFORE ${CMAKE_SOURCE_DIR}/kdecore
)

add_definitions(-Wno-undef -Wno-unused-parameter)

set(DumpRenderTree_SRCS main.cpp DumpRenderTree.cpp DumpRenderTreeClient.cpp)
kde4_automoc(${DumpRenderTree_SRCS})
kde4_add_executable(DumpRenderTree RUN_UNINSTALLED ${DumpRenderTree_SRCS})
target_link_libraries(DumpRenderTree ${KDE4_KDECORE_LIBS} WebCore-unity)
