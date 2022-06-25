macro(WEBKIT_BUILD_MODERN_MEDIA_CONTROLS_GRESOURCES _derived_sources_dir)
    add_custom_command(
        OUTPUT ${_derived_sources_dir}/ModernMediaControlsGResourceBundle.xml
        DEPENDS ${ModernMediaControlsImageFiles}
                ${TOOLS_DIR}/glib/generate-modern-media-controls-gresource-manifest.py
        COMMAND ${PYTHON_EXECUTABLE} ${TOOLS_DIR}/glib/generate-modern-media-controls-gresource-manifest.py --input=${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita --output=${_derived_sources_dir}/ModernMediaControlsGResourceBundle.xml
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${_derived_sources_dir}/ModernMediaControlsGResourceBundle.c ${_derived_sources_dir}/ModernMediaControlsGResourceBundle.deps
        DEPENDS ${_derived_sources_dir}/ModernMediaControlsGResourceBundle.xml
        DEPFILE {_derived_sources_dir}/ModernMediaControlsGResourceBundle.deps
        COMMAND glib-compile-resources --generate --sourcedir=${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita --target=${_derived_sources_dir}/ModernMediaControlsGResourceBundle.c --dependency-file=${_derived_sources_dir}/ModernMediaControlsGResourceBundle.deps ${_derived_sources_dir}/ModernMediaControlsGResourceBundle.xml
        VERBATIM
    )
endmacro()
