macro(WEBKIT_BUILD_MODERN_MEDIA_CONTROLS_GRESOURCES _derived_sources_dir)
    add_custom_command(
        OUTPUT ${_derived_sources_dir}/ModernMediaControlsGResourceBundle.xml
        DEPENDS ${ModernMediaControlsImageFiles}
                ${TOOLS_DIR}/glib/generate-modern-media-controls-gresource-manifest.py
        COMMAND ${PYTHON_EXECUTABLE} ${TOOLS_DIR}/glib/generate-modern-media-controls-gresource-manifest.py --input=${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita --output=${_derived_sources_dir}/ModernMediaControlsGResourceBundle.xml
        VERBATIM
    )

    GLIB_COMPILE_RESOURCES(
        OUTPUT        ${_derived_sources_dir}/ModernMediaControlsGResourceBundle.c
        SOURCE_XML    ${_derived_sources_dir}/ModernMediaControlsGResourceBundle.xml
        RESOURCE_DIRS ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita
    )
endmacro()
