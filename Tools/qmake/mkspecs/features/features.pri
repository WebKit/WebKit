# -------------------------------------------------------------------
# This file contains a static list of default values for all the
# ENABLE_FOO features of WebKit.
#
# If a feature is enabled, it most likely does not have any detection
# in features.prf except basic sanitazion. If a feature is disabled it
# will have detection in features.prf, unless it's something we
# completely disable.
#
# FIXME: Add warning about auto-generating when Features.py land
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

FEATURE_DEFAULTS = \
    ENABLE_3D_RENDERING=1 \
    ENABLE_ANIMATION_API=0 \
    ENABLE_BLOB=1 \
    ENABLE_CHANNEL_MESSAGING=1 \
    ENABLE_CSS_FILTERS=1 \
    ENABLE_DASHBOARD_SUPPORT=0 \
    ENABLE_DATALIST=1 \
    ENABLE_DETAILS=1 \
    ENABLE_DIRECTORY_UPLOAD=0 \
    ENABLE_FAST_MOBILE_SCROLLING=1 \
    ENABLE_FILE_SYSTEM=0 \
    ENABLE_FILTERS=1 \
    ENABLE_FTPDIR=1 \
    ENABLE_FULLSCREEN_API=0 \
    ENABLE_GAMEPAD=0 \
    ENABLE_GEOLOCATION=0 \
    ENABLE_GESTURE_EVENTS=1 \
    ENABLE_ICONDATABASE=1 \
    ENABLE_INPUT_SPEECH=0 \
    ENABLE_INPUT_TYPE_COLOR=0 \
    ENABLE_INSPECTOR=1 \
    ENABLE_JAVASCRIPT_DEBUGGER=1 \
    ENABLE_LEGACY_NOTIFICATIONS=1 \
    ENABLE_LEGACY_WEBKIT_BLOB_BUILDER=1 \
    ENABLE_MEDIA_SOURCE=0 \
    ENABLE_MEDIA_STATISTICS=0 \
    ENABLE_MEDIA_STREAM=0 \
    ENABLE_METER_TAG=1 \
    ENABLE_MHTML=0 \
    ENABLE_MICRODATA=0 \
    ENABLE_NETSCAPE_PLUGIN_API=0 \
    ENABLE_NOTIFICATIONS=1 \
    ENABLE_PAGE_VISIBILITY_API=1 \
    ENABLE_PROGRESS_TAG=1 \
    ENABLE_QUOTA=0 \
    ENABLE_REQUEST_ANIMATION_FRAME=1 \
    ENABLE_SCRIPTED_SPEECH=0 \
    ENABLE_SHADOW_DOM=0 \
    ENABLE_SHARED_WORKERS=1 \
    ENABLE_SQL_DATABASE=1 \
    ENABLE_SVG=1 \
    ENABLE_SVG_FONTS=0 \
    ENABLE_TOUCH_ADJUSTMENT=1 \
    ENABLE_TOUCH_EVENTS=1 \
    ENABLE_TOUCH_ICON_LOADING=0 \
    ENABLE_VIDEO=0 \
    ENABLE_VIDEO_TRACK=0 \
    ENABLE_WEBGL=0 \
    ENABLE_WEB_AUDIO=0 \
    ENABLE_WEB_SOCKETS=1 \
    ENABLE_WEB_TIMING=1 \
    ENABLE_WORKERS=1 \
    ENABLE_XSLT=0
