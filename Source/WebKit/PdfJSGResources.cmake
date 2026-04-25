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

    GLIB_COMPILE_RESOURCES(
        OUTPUT        ${_derived_sources_dir}/PdfJSGResourceBundle.c
        SOURCE_XML    ${_derived_sources_dir}/PdfJSGResourceBundle.xml
        RESOURCE_DIRS ${THIRDPARTY_DIR}/pdfjs
    )

    add_custom_command(
        OUTPUT ${_derived_sources_dir}/PdfJSGResourceBundleExtras.xml
        DEPENDS ${TOOLS_DIR}/glib/generate-pdfjs-resource-manifest.py ${PdfJSExtraFiles}
        COMMAND ${CMAKE_COMMAND} -E remove -f ${_derived_sources_dir}/PdfJSGResourceBundleExtras.deps
        COMMAND ${PYTHON_EXECUTABLE} ${TOOLS_DIR}/glib/generate-pdfjs-resource-manifest.py --gresource --input=${WEBCORE_DIR}/Modules/pdfjs-extras --output=${_derived_sources_dir}/PdfJSGResourceBundleExtras.xml
        VERBATIM
    )

    GLIB_COMPILE_RESOURCES(
        OUTPUT        ${_derived_sources_dir}/PdfJSGResourceBundleExtras.c
        SOURCE_XML    ${_derived_sources_dir}/PdfJSGResourceBundleExtras.xml
        RESOURCE_DIRS ${WEBCORE_DIR}/Modules/pdfjs-extras
    )
endmacro()
