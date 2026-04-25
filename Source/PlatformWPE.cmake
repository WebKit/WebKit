include(WebKitDist)

if (DEVELOPER_MODE)
    WEBKIT_DECLARE_DIST_TARGETS(WPE wpewebkit ${TOOLS_DIR}/wpe/manifest.txt.in)
endif ()
