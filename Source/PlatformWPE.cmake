include(GtkDoc)
include(WebKitDist)

list(APPEND DocumentationDependencies
    WebKit
    "${CMAKE_SOURCE_DIR}/Source/WebKit/UIProcess/API/wpe/docs/wpe-docs.sgml"
    "${CMAKE_SOURCE_DIR}/Source/WebKit/WebProcess/InjectedBundle/API/wpe/docs/wpe-webextensions-docs.sgml"
    "${CMAKE_SOURCE_DIR}/Source/WebKit/UIProcess/API/wpe/docs/wpe-${WPE_API_VERSION}-sections.txt"
    "${CMAKE_SOURCE_DIR}/Source/WebKit/WebProcess/InjectedBundle/API/wpe/docs/wpe-webextensions-${WPE_API_VERSION}-sections.txt"
)

if (ENABLE_GTKDOC)
    install(DIRECTORY ${CMAKE_BINARY_DIR}/Documentation/wpe-${WPE_API_VERSION}/html/wpe-${WPE_API_VERSION}
            DESTINATION "${CMAKE_INSTALL_DATADIR}/gtk-doc/html"
    )
    install(DIRECTORY ${CMAKE_BINARY_DIR}/Documentation/wpe-webextensions-${WPE_API_VERSION}/html/wpe-webextensions-${WPE_API_VERSION}
        DESTINATION "${CMAKE_INSTALL_DATADIR}/gtk-doc/html"
    )
endif ()

ADD_GTKDOC_GENERATOR("docs-build.stamp" "--wpe")
if (ENABLE_GTKDOC)
    add_custom_target(gtkdoc ALL DEPENDS "${CMAKE_BINARY_DIR}/docs-build.stamp")
elseif (NOT ENABLED_COMPILER_SANITIZERS AND NOT CMAKE_CROSSCOMPILING AND NOT APPLE)
    add_custom_target(gtkdoc DEPENDS "${CMAKE_BINARY_DIR}/docs-build.stamp")

    # Add a default build step which check that documentation does not have any warnings
    # or errors. This is useful to prevent breaking documentation inadvertently during
    # the course of development.
    if (DEVELOPER_MODE)
        ADD_GTKDOC_GENERATOR("docs-build-no-html.stamp" "--wpe;--skip-html")
        add_custom_target(gtkdoc-no-html ALL DEPENDS "${CMAKE_BINARY_DIR}/docs-build-no-html.stamp")
    endif ()
endif ()

if (DEVELOPER_MODE)
    add_custom_target(Documentation DEPENDS gtkdoc)
    WEBKIT_DECLARE_DIST_TARGETS(WPE wpewebkit ${TOOLS_DIR}/wpe/manifest.txt.in)
endif ()
