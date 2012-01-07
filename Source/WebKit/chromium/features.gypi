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
      'ENABLE_BLOB=1',
      'ENABLE_BLOB_SLICE=1',
      'ENABLE_CHANNEL_MESSAGING=1',
      'ENABLE_CLIENT_BASED_GEOLOCATION=1',
      'ENABLE_CSS_FILTERS=1',
      'ENABLE_DASHBOARD_SUPPORT=0',
      'ENABLE_DATA_TRANSFER_ITEMS=1',
      'ENABLE_DETAILS=1',
      'ENABLE_DEVICE_ORIENTATION=1',
      'ENABLE_DIRECTORY_UPLOAD=1',
      'ENABLE_DOWNLOAD_ATTRIBUTE=1',
      'ENABLE_FILE_SYSTEM=1',
      'ENABLE_FILTERS=1',
      'ENABLE_FULLSCREEN_API=1',
      'ENABLE_GAMEPAD=1',
      'ENABLE_GEOLOCATION=1',
      'ENABLE_GESTURE_EVENTS=1',
      'ENABLE_GESTURE_RECOGNIZER=1',
      'ENABLE_ICONDATABASE=0',
      'ENABLE_INDEXED_DATABASE=1',
      'ENABLE_INPUT_COLOR=0',
      'ENABLE_INPUT_SPEECH=1',
      'ENABLE_INPUT_TYPE_COLOR=0',
      'ENABLE_INPUT_TYPE_DATE=0',
      'ENABLE_INPUT_TYPE_DATETIME=0',
      'ENABLE_INPUT_TYPE_DATETIMELOCAL=0',
      'ENABLE_INPUT_TYPE_MONTH=0',
      'ENABLE_INPUT_TYPE_TIME=0',
      'ENABLE_INPUT_TYPE_WEEK=0',
      'ENABLE_JAVASCRIPT_DEBUGGER=1',
      'ENABLE_JAVASCRIPT_I18N_API=1',
      'ENABLE_LINK_PREFETCH=1',
      'ENABLE_MEDIA_SOURCE=1',
      'ENABLE_MEDIA_STATISTICS=1',
      'ENABLE_MEDIA_STREAM=1',
      'ENABLE_METER_TAG=1',
      'ENABLE_MHTML=1',
      'ENABLE_MICRODATA=0',
      'ENABLE_MUTATION_OBSERVERS=<(enable_mutation_observers)',
      'ENABLE_NOTIFICATIONS=1',
      'ENABLE_ORIENTATION_EVENTS=0',
      'ENABLE_PAGE_VISIBILITY_API=1',
      'ENABLE_POINTER_LOCK=1',
      'ENABLE_PROGRESS_TAG=1',
      'ENABLE_QUOTA=1',
      'ENABLE_REQUEST_ANIMATION_FRAME=1',
      'ENABLE_RUBY=1',
      'ENABLE_SANDBOX=1',
      'ENABLE_SHARED_WORKERS=1',
      'ENABLE_SMOOTH_SCROLLING=1',
      'ENABLE_SQL_DATABASE=1',
      'ENABLE_STYLE_SCOPED=0',
      'ENABLE_SVG=<(enable_svg)',
      'ENABLE_SVG_FONTS=<(enable_svg)',
      'ENABLE_TOUCH_EVENTS=<(enable_touch_events)',
      'ENABLE_TOUCH_ICON_LOADING=<(enable_touch_icon_loading)',
      'ENABLE_V8_SCRIPT_DEBUG_SERVER=1',
      'ENABLE_VIDEO=1',
      'ENABLE_VIDEO_TRACK=1',
      'ENABLE_VIEWPORT=<(enable_viewport)',
      'ENABLE_WEBGL=1',
      'ENABLE_WEB_SOCKETS=1',
      'ENABLE_WEB_TIMING=1',
      'ENABLE_WORKERS=1',
      'ENABLE_XHR_RESPONSE_BLOB=1',
      'ENABLE_XSLT=1',
      'WTF_USE_LEVELDB=1',
      'WTF_USE_BUILTIN_UTF8_CODEC=1',
      # WTF_USE_DYNAMIC_ANNOTATIONS=1 may be defined in build/common.gypi
      # We can't define it here because it should be present only
      # in Debug or release_valgrind_build=1 builds.
      'WTF_USE_OPENTYPE_SANITIZER=1',
      'WTF_USE_SKIA_TEXT=<(enable_skia_text)',
      'WTF_USE_WEBP=1',
      'WTF_USE_WEBKIT_IMAGE_DECODERS=1',
    ],
    # We have to nest variables inside variables so that they can be overridden
    # through GYP_DEFINES.
    'variables': {
      'use_accelerated_compositing%': 1,
      'enable_skia_text%': 1,
      'enable_svg%': 1,
      'enable_viewport%': 0,
      'enable_touch_events%': 1,
      'use_skia%': 0,
      'enable_touch_icon_loading%' : 0,
      'enable_mutation_observers%': 1,
    },
    'use_accelerated_compositing%': '<(use_accelerated_compositing)',
    'enable_skia_text%': '<(enable_skia_text)',
    'enable_svg%': '<(enable_svg)',
    'enable_touch_events%': '<(enable_touch_events)',
    'use_skia%': '<(use_skia)',
    'conditions': [
      ['OS=="android"', {
        'feature_defines': [
          'ENABLE_WEB_AUDIO=0',
        ],
      }, {
        'feature_defines': [
          'ENABLE_WEB_AUDIO=1',
        ],
      }],
      ['use_accelerated_compositing==1', {
        'feature_defines': [
          'WTF_USE_ACCELERATED_COMPOSITING=1',
          'ENABLE_3D_RENDERING=1',
        ],
      }],
      ['use_accelerated_compositing==1 and (OS!="mac" or use_skia==1)', {
        'feature_defines': [
          'ENABLE_ACCELERATED_2D_CANVAS=1',
        ],
      }],
      # Mac OS X uses Accelerate.framework FFT by default instead of FFmpeg.
      ['OS!="mac"', {
        'feature_defines': [
          'WTF_USE_WEBAUDIO_FFMPEG=1',
        ],
        'use_skia%': 1,
      }],
      ['enable_register_protocol_handler==1', {
        'feature_defines': [
          'ENABLE_REGISTER_PROTOCOL_HANDLER=1',
        ],
      }],
      ['enable_web_intents==1', {
        'feature_defines': [
          'ENABLE_WEB_INTENTS=1',
        ],
      }],
      ['OS=="mac"', {
        'feature_defines': [
          'ENABLE_RUBBER_BANDING=1',
          'WTF_USE_SKIA_ON_MAC_CHROMIUM=<(use_skia)',
        ],
      }],
    ],
  },
}
