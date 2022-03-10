file(GLOB_RECURSE PdfJSFiles ${THIRDPARTY_DIR}/pdfjs/*)

set(PdfJSExtraFiles
    ${WEBCORE_DIR}/Modules/pdfjs-extras/content-script.js
)

macro(WEBKIT_BUILD_PDFJS_GRESOURCES _derived_sources_dir)
    add_custom_command(
        OUTPUT ${_derived_sources_dir}/PdfJSGResourceBundle.xml
        DEPENDS ${TOOLS_DIR}/glib/generate-pdfjs-gresource-manifest.py
                ${PdfJSFiles}
        COMMAND ${PYTHON_EXECUTABLE} ${TOOLS_DIR}/glib/generate-pdfjs-gresource-manifest.py --input=${THIRDPARTY_DIR}/pdfjs --output=${_derived_sources_dir}/PdfJSGResourceBundle.xml
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${_derived_sources_dir}/PdfJSGResourceBundle.c
        DEPENDS ${_derived_sources_dir}/PdfJSGResourceBundle.xml
        COMMAND glib-compile-resources --generate --sourcedir=${THIRDPARTY_DIR}/pdfjs --target=${_derived_sources_dir}/PdfJSGResourceBundle.c ${_derived_sources_dir}/PdfJSGResourceBundle.xml
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${_derived_sources_dir}/PdfJSGResourceBundleExtras.xml
        DEPENDS ${TOOLS_DIR}/glib/generate-pdfjs-gresource-manifest.py
                ${PdfJSExtraFiles}
        COMMAND ${PYTHON_EXECUTABLE} ${TOOLS_DIR}/glib/generate-pdfjs-gresource-manifest.py --input=${WEBCORE_DIR}/Modules/pdfjs-extras --output=${_derived_sources_dir}/PdfJSGResourceBundleExtras.xml
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${_derived_sources_dir}/PdfJSGResourceBundleExtras.c
        DEPENDS ${_derived_sources_dir}/PdfJSGResourceBundleExtras.xml
        COMMAND glib-compile-resources --generate --sourcedir=${WEBCORE_DIR}/Modules/pdfjs-extras --target=${_derived_sources_dir}/PdfJSGResourceBundleExtras.c ${_derived_sources_dir}/PdfJSGResourceBundleExtras.xml
        VERBATIM
    )
endmacro()
