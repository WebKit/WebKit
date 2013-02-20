#
# Copyright (C) 2009 Google Inc. All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

{
  # The following defines turn WebKit features on and off.
  'variables': {
    'feature_defines': [
      'ENABLE_3D_PLUGIN=1',
      'ENABLE_BATTERY_STATUS=0',
      'ENABLE_BLOB=1',
      'ENABLE_BLOB_SLICE=1',
      'ENABLE_CANVAS_PATH=1',
      'ENABLE_CANVAS_PROXY=1',
      'ENABLE_CHANNEL_MESSAGING=1',
      'ENABLE_CSP_NEXT=1',
      'ENABLE_CSS3_CONDITIONAL_RULES=0',
      'ENABLE_CSS3_TEXT=0',
      'ENABLE_CSS_BOX_DECORATION_BREAK=1',
      'ENABLE_CSS_COMPOSITING=0',
      'ENABLE_CSS_DEVICE_ADAPTATION=0',
      'ENABLE_CSS_EXCLUSIONS=1',
      'ENABLE_CSS_FILTERS=1',
      'ENABLE_CSS_IMAGE_SET=1',
      'ENABLE_CSS_IMAGE_RESOLUTION=0',
      'ENABLE_CSS_REGIONS=1',
      'ENABLE_CSS_SHADERS=1',
      'ENABLE_CSS_TRANSFORMS_ANIMATIONS_UNPREFIXED=0',
      'ENABLE_CSS_VARIABLES=1',
      'ENABLE_CSS_STICKY_POSITION=1',
      'ENABLE_CUSTOM_SCHEME_HANDLER=0',
      'ENABLE_DASHBOARD_SUPPORT=0',
      'ENABLE_DATA_TRANSFER_ITEMS=1',
      'ENABLE_DETAILS_ELEMENT=1',
      'ENABLE_DEVICE_ORIENTATION=1',
      'ENABLE_DIALOG_ELEMENT=1',
      'ENABLE_DIRECTORY_UPLOAD=1',
      'ENABLE_DOM4_EVENTS_CONSTRUCTOR=1',
      'ENABLE_DOWNLOAD_ATTRIBUTE=1',
      'ENABLE_DRAGGABLE_REGION=1',
      'ENABLE_ENCRYPTED_MEDIA=1',
      'ENABLE_FILE_SYSTEM=1',
      'ENABLE_FILTERS=1',
      'ENABLE_FULLSCREEN_API=1',
      'ENABLE_GAMEPAD=1',
      'ENABLE_GEOLOCATION=1',
      'ENABLE_GESTURE_EVENTS=1',
      'ENABLE_ICONDATABASE=0',
      'ENABLE_IFRAME_SEAMLESS=1',
      'ENABLE_INDEXED_DATABASE=1',
      'ENABLE_INPUT_TYPE_DATE=1',
      'ENABLE_INPUT_TYPE_DATETIMELOCAL=1',
      'ENABLE_INPUT_TYPE_MONTH=1',
      'ENABLE_INPUT_TYPE_TIME=1',
      'ENABLE_JAVASCRIPT_DEBUGGER=1',
      'ENABLE_JAVASCRIPT_I18N_API=1',
      'ENABLE_LEGACY_CSS_VENDOR_PREFIXES=0',
      'ENABLE_LEGACY_VIEWPORT_ADAPTION=1',
      'ENABLE_LEGACY_VENDOR_PREFIXES=0',
      'ENABLE_LEGACY_WEB_AUDIO=1',
      'ENABLE_LINK_PREFETCH=1',
      'ENABLE_LINK_PRERENDER=1',
      'ENABLE_MATHML=0',
      'ENABLE_MEDIA_SOURCE=1',
      'ENABLE_MEDIA_STATISTICS=1',
      'ENABLE_MEDIA_STREAM=<(enable_webrtc)',
      'ENABLE_METER_ELEMENT=1',
      'ENABLE_MHTML=1',
      'ENABLE_MICRODATA=0',
      'ENABLE_MOUSE_CURSOR_SCALE=1',
      'ENABLE_NAVIGATOR_CONTENT_UTILS=1',
      'ENABLE_NOSNIFF=1',
      'ENABLE_PAGE_VISIBILITY_API=1',
      'ENABLE_PERFORMANCE_TIMELINE=1',
      'ENABLE_POINTER_LOCK=1',
      'ENABLE_PROGRESS_ELEMENT=1',
      'ENABLE_PROXIMITY_EVENTS=0',
      'ENABLE_QUOTA=1',
      'ENABLE_REQUEST_ANIMATION_FRAME=1',
      'ENABLE_REQUEST_AUTOCOMPLETE=1',
      'ENABLE_RESOLUTION_MEDIA_QUERY=0',
      'ENABLE_RESOURCE_TIMING=1',
      'ENABLE_RUBY=1',
      'ENABLE_SANDBOX=1',
      'ENABLE_SCRIPTED_SPEECH=1',
      'ENABLE_SHADOW_DOM=1',
      'ENABLE_SMOOTH_SCROLLING=1',
      'ENABLE_SPEECH_SYNTHESIS=0',
      'ENABLE_SQL_DATABASE=<(enable_sql_database)',
      'ENABLE_STYLE_SCOPED=1',
      'ENABLE_SUBPIXEL_LAYOUT=1',
      'ENABLE_SVG=<(enable_svg)',
      'ENABLE_SVG_FONTS=<(enable_svg)',
      'ENABLE_TEMPLATE_ELEMENT=1',
      'ENABLE_TEXT_AUTOSIZING=1',
      'ENABLE_THREADED_HTML_PARSER=1',
      'ENABLE_TOUCH_ADJUSTMENT=1',
      'ENABLE_TOUCH_EVENTS=<(enable_touch_events)',
      'ENABLE_TOUCH_EVENT_TRACKING=<(enable_touch_events)',
      'ENABLE_TOUCH_ICON_LOADING=<(enable_touch_icon_loading)',
      'ENABLE_TOUCH_SLIDER=1',
      'ENABLE_USER_TIMING=1',
      'ENABLE_V8_SCRIPT_DEBUG_SERVER=1',
      'ENABLE_VIDEO=1',
      'ENABLE_VIDEO_TRACK=1',
      'ENABLE_VIEWPORT=1',
      'ENABLE_WEBGL=1',
      'ENABLE_WEB_SOCKETS=1',
      'ENABLE_WEB_TIMING=1',
      'ENABLE_WORKERS=1',
      'ENABLE_XHR_TIMEOUT=0',
      'ENABLE_XSLT=1',
      'WTF_USE_LEVELDB=1',
      'WTF_USE_BUILTIN_UTF8_CODEC=1',
      # WTF_USE_DYNAMIC_ANNOTATIONS=1 may be defined in build/common.gypi
      # We can't define it here because it should be present only
      # in Debug or release_valgrind_build=1 builds.
      'WTF_USE_OPENTYPE_SANITIZER=1',
      'WTF_USE_RTL_SCROLLBAR=1',
      'WTF_USE_SKIA_TEXT=<(enable_skia_text)',
      'WTF_USE_WEBP=1',
      'WTF_USE_WEBKIT_IMAGE_DECODERS=1',
    ],
    # We have to nest variables inside variables so that they can be overridden
    # through GYP_DEFINES.
    'variables': {
      'use_accelerated_compositing%': 1,
      'enable_skia_text%': 1,
      'enable_sql_database%': 1,
      'enable_svg%': 1,
      'enable_touch_events%': 1,
      'enable_touch_icon_loading%' : 0,
    },
    'use_accelerated_compositing%': '<(use_accelerated_compositing)',
    'enable_skia_text%': '<(enable_skia_text)',
    'enable_sql_database%': '<(enable_sql_database)',
    'enable_svg%': '<(enable_svg)',
    'enable_touch_events%': '<(enable_touch_events)',
    'conditions': [
      ['OS=="android"', {
        'feature_defines': [
          'ENABLE_CALENDAR_PICKER=0',
          'ENABLE_DATALIST_ELEMENT=0',
          'ENABLE_FAST_MOBILE_SCROLLING=1',
          'ENABLE_INPUT_SPEECH=0',
          'ENABLE_INPUT_TYPE_COLOR=0',
          'ENABLE_INPUT_TYPE_WEEK=0',
          'ENABLE_LEGACY_NOTIFICATIONS=0',
          'ENABLE_MEDIA_CAPTURE=1',
          'ENABLE_NOTIFICATIONS=0',
          'ENABLE_ORIENTATION_EVENTS=1',
          'ENABLE_PAGE_POPUP=0',
          'ENABLE_PRINTING=0',
          'ENABLE_REGISTER_PROTOCOL_HANDLER=0',
          # FIXME: Disable once the linking error has been resolved.
          # https://bugs.webkit.org/show_bug.cgi?id=88636
          'ENABLE_SHARED_WORKERS=1',
          'ENABLE_WEB_AUDIO=0',
          'WTF_USE_NATIVE_FULLSCREEN_VIDEO=1',
        ],
        'enable_touch_icon_loading': 1,
      }, { # OS!="android"
        'feature_defines': [
          'ENABLE_CALENDAR_PICKER=1',
          'ENABLE_DATALIST_ELEMENT=1',
          'ENABLE_INPUT_SPEECH=1',
          'ENABLE_INPUT_TYPE_COLOR=1',
          'ENABLE_INPUT_TYPE_WEEK=1',
          'ENABLE_INPUT_MULTIPLE_FIELDS_UI=1',
          'ENABLE_LEGACY_NOTIFICATIONS=1',
          'ENABLE_MEDIA_CAPTURE=0',
          'ENABLE_NOTIFICATIONS=1',
          'ENABLE_ORIENTATION_EVENTS=0',
          'ENABLE_PAGE_POPUP=1',
          'ENABLE_PRINTING=1',
          'ENABLE_SHARED_WORKERS=1',
          'ENABLE_WEB_AUDIO=1',
        ],
      }],
      ['OS=="linux" or OS=="mac"', {
        'feature_defines': [
          'ENABLE_BINDING_INTEGRITY=1',
        ],
      }, { # OS!="linux"
        'feature_defines': [
          'ENABLE_BINDING_INTEGRITY=0',
        ],
      }],
      ['use_accelerated_compositing==1', {
        'feature_defines': [
          'ENABLE_3D_RENDERING=1',
          'ENABLE_ACCELERATED_2D_CANVAS=1',
          'WTF_USE_ACCELERATED_COMPOSITING=1',
        ],
      }],
      # Mac OS X uses Accelerate.framework FFT by default instead of FFmpeg.
      ['OS!="mac" and OS!="android"', {
        'feature_defines': [
          'WTF_USE_WEBAUDIO_FFMPEG=1',
        ],
      }],
      ['OS=="win" or OS=="android" or use_x11==1', {
        'feature_defines': [
          'ENABLE_OPENTYPE_VERTICAL=1',
        ],
      }],
      ['OS=="mac"', {
        'feature_defines': [
          'ENABLE_RUBBER_BANDING=1',
          'WTF_USE_SKIA_ON_MAC_CHROMIUM=1',
        ],
      }],
      ['use_x11==1 or OS=="android"', {
        'feature_defines': [
          'WTF_USE_HARFBUZZ=1',
        ],
      }],
      ['chromeos==1', {
        'feature_defines': [
          'SK_SUPPORT_HINTING_SCALE_FACTOR',
        ],
      }],
      ['use_default_render_theme==1', {
        'feature_defines': [
          'ENABLE_DEFAULT_RENDER_THEME=1',
        ],
      }],
    ],
  },
}
