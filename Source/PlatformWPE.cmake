include(WebKitDist)

if (DEVELOPER_MODE)
    # FIXME: This should depend on a gtkdoc target
    add_custom_target(Documentation)
    WEBKIT_DECLARE_DIST_TARGETS(WPE wpewebkit ${TOOLS_DIR}/wpe/manifest.txt.in)
endif ()
