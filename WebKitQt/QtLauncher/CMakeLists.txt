include_directories(
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../..//JavaScriptCore
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/bridge
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/page
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/loader
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/platform
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/platform/graphics
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/platform/graphics/svg
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/platform/network
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/platform/qt
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/ksvg2
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/ksvg2/svg
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/ksvg2/css
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/rendering
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/editing
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/html
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/css
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/dom
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/xml
)

add_definitions(-Wno-undef -Wno-unused-parameter)

set(testunity_SRCS main.cpp)
IF (WEBKIT_USE_KDE_SUPPORT)
    kde4_add_executable(testunity RUN_UNINSTALLED ${testunity_SRCS})
    target_link_libraries(testunity  ${KDE4_KDECORE_LIBS} WebCore-unity)
    kde4_automoc(testunity ${testunity_SRCS})
ELSE (WEBKIT_USE_KDE_SUPPORT)
    include_directories(${QT_INCLUDES})
    add_executable(testunity ${testunity_SRCS})
    target_link_libraries(testunity WebCore-unity)
    qt4_automoc(testunity ${testunity_SRCS})
ENDIF (WEBKIT_USE_KDE_SUPPORT)
