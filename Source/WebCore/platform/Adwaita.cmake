list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/adwaita"

    "${WEBCORE_DIR}/platform/graphics/adwaita"
)

list(APPEND WebCore_UNIFIED_SOURCE_LIST_FILES
    "platform/SourcesAdwaita.txt"
)

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    platform/adwaita/ScrollbarThemeAdwaita.h
    platform/adwaita/ThemeAdwaita.h

    platform/graphics/adwaita/Adwaita.h
    platform/graphics/adwaita/ButtonControlAdwaita.h
    platform/graphics/adwaita/ControlAdwaita.h
    platform/graphics/adwaita/ControlFactoryAdwaita.h
    platform/graphics/adwaita/InnerSpinButtonAdwaita.h
    platform/graphics/adwaita/MenuListAdwaita.h
    platform/graphics/adwaita/ProgressBarAdwaita.h
    platform/graphics/adwaita/SliderThumbAdwaita.h
    platform/graphics/adwaita/SliderTrackAdwaita.h
    platform/graphics/adwaita/TextFieldAdwaita.h
    platform/graphics/adwaita/ToggleButtonAdwaita.h

    rendering/adwaita/RenderThemeAdwaita.h
)

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/css/themeAdwaita.css
)

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WebCore_DERIVED_SOURCES_DIR}/ModernMediaControls.css
)

list(APPEND WebCore_USER_AGENT_SCRIPTS
    ${WebCore_DERIVED_SOURCES_DIR}/ModernMediaControls.js
)

set(WebCore_USER_AGENT_SCRIPTS_DEPENDENCIES ${WEBCORE_DIR}/rendering/adwaita/RenderThemeAdwaita.cpp)

set(ModernMediaControlsImageFiles
    ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/EnterFullscreen.svg
    ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/ExitFullscreen.svg
    ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/Forward.svg
    ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/invalid-placard@1x.png
    ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/invalid-placard@2x.png
    ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/MediaSelector.svg
    ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/MediaSelector-fullscreen.svg
    ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/Overflow.svg
    ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/Pause.svg
    ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/PipIn.svg
    ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/PipIn-fullscreen.svg
    ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/PipOut.svg
    ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/Play.svg
    ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/Rewind.svg
    ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/SkipBack10.svg
    ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/SkipBack15.svg
    ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/SkipForward10.svg
    ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/SkipForward15.svg
    ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/Volume0-RTL.svg
    ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/Volume0.svg
    ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/Volume1-RTL.svg
    ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/Volume1.svg
    ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/Volume2-RTL.svg
    ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/Volume2.svg
    ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/Volume3-RTL.svg
    ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/Volume3.svg
    ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/VolumeMuted-RTL.svg
    ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/VolumeMuted.svg
    ${WEBCORE_DIR}/Modules/modern-media-controls/images/adwaita/X.svg
)
