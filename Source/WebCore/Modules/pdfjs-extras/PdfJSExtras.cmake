set(PdfJSExtraFiles
    ${WEBCORE_DIR}/Modules/pdfjs-extras/content-script.js
)

if (USE_THEME_ADWAITA)
    list(APPEND PdfJSExtraFiles
        ${WEBCORE_DIR}/Modules/pdfjs-extras/adwaita/style.css
    )
endif ()
