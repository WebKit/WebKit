set(PdfJSExtraFiles
    ${WEBCORE_DIR}/Modules/pdfjs-extras/content-script.js
)

if (PORT STREQUAL "GTK" OR PORT STREQUAL "WPE")
    list(APPEND PdfJSExtraFiles
        ${WEBCORE_DIR}/Modules/pdfjs-extras/adwaita/style.css
    )
endif ()
