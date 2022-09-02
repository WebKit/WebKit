include(../ThirdParty/pdfjs/PdfJSFiles.cmake)
include(../WebCore/Modules/pdfjs-extras/PdfJSExtras.cmake)

macro(WEBKIT_BUILD_PDFJS_GRESOURCES _derived_sources_dir)
    add_custom_command(
        OUTPUT ${_derived_sources_dir}/PdfJSGResourceBundle.xml
        DEPENDS ${TOOLS_DIR}/glib/generate-pdfjs-resource-manifest.py ${PdfJSFiles}
        COMMAND ${CMAKE_COMMAND} -E remove -f ${_derived_sources_dir}/PdfJSGResourceBundle.deps
        COMMAND ${PYTHON_EXECUTABLE} ${TOOLS_DIR}/glib/generate-pdfjs-resource-manifest.py --gresource --input=${THIRDPARTY_DIR}/pdfjs --output=${_derived_sources_dir}/PdfJSGResourceBundle.xml
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${_derived_sources_dir}/PdfJSGResourceBundle.c ${_derived_sources_dir}/PdfJSGResourceBundle.deps
        DEPENDS ${_derived_sources_dir}/PdfJSGResourceBundle.xml
        DEPFILE ${_derived_sources_dir}/PdfJSGResourceBundle.deps
        COMMAND glib-compile-resources --generate --sourcedir=${THIRDPARTY_DIR}/pdfjs --target=${_derived_sources_dir}/PdfJSGResourceBundle.c --dependency-file=${_derived_sources_dir}/PdfJSGResourceBundle.deps ${_derived_sources_dir}/PdfJSGResourceBundle.xml
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${_derived_sources_dir}/PdfJSGResourceBundleExtras.xml
        DEPENDS ${TOOLS_DIR}/glib/generate-pdfjs-resource-manifest.py ${PdfJSExtraFiles}
        COMMAND ${CMAKE_COMMAND} -E remove -f ${_derived_sources_dir}/PdfJSGResourceBundleExtras.deps
        COMMAND ${PYTHON_EXECUTABLE} ${TOOLS_DIR}/glib/generate-pdfjs-resource-manifest.py --gresource --input=${WEBCORE_DIR}/Modules/pdfjs-extras --output=${_derived_sources_dir}/PdfJSGResourceBundleExtras.xml
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${_derived_sources_dir}/PdfJSGResourceBundleExtras.c ${_derived_sources_dir}/PdfJSGResourceBundleExtras.deps
        DEPENDS ${_derived_sources_dir}/PdfJSGResourceBundleExtras.xml
        DEPFILE ${_derived_sources_dir}/PdfJSGResourceBundleExtras.deps
        COMMAND glib-compile-resources --generate --sourcedir=${WEBCORE_DIR}/Modules/pdfjs-extras --target=${_derived_sources_dir}/PdfJSGResourceBundleExtras.c ${_derived_sources_dir}/PdfJSGResourceBundleExtras.xml
        VERBATIM
    )
endmacro()
