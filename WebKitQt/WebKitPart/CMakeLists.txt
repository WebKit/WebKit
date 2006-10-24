include_directories (
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../JavaScriptCore
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/bridge
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/page
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/loader
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/platform
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/platform/qt
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/ksvg2
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/ksvg2/css
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/ksvg2/svg
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/rendering
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/editing
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/html
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/css
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/dom
   BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../../WebCore/xml
   BEFORE ${CMAKE_SOURCE_DIR}/kdecore
)

add_definitions (-Wno-undef -Wno-unused-parameter)

set (WebKitPart_SRCS WebKitFactory.cpp WebKitPart.cpp WebKitPartBrowserExtension.cpp WebKitPartInterface.cpp WebKitPartClient.cpp)

qt4_add_dbus_adaptor (WebKitPart_SRCS org.kde.WebKitPart.xml WebKitPartInterface.h WebKitPartInterface)

kde4_automoc (WebKitPart ${WebKitPart_SRCS})
kde4_add_plugin (WebKitPart WITH_PREFIX ${WebKitPart_SRCS})
target_link_libraries (WebKitPart ${KDE4_KDECORE_LIBS} kparts WebCore-unity)

kde4_install_libtool_file (${PLUGIN_INSTALL_DIR}/ WebKitPart)
install (TARGETS WebKitPart DESTINATION ${PLUGIN_INSTALL_DIR})

install (FILES WebKitPart.desktop DESTINATION ${SERVICES_INSTALL_DIR})
install (FILES WebKitPart.rc WebKitPartBrowser.rc DESTINATION ${DATA_INSTALL_DIR}/WebKitPart)
