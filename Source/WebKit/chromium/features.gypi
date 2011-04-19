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
  # The following defines turn webkit features on and off.
  'variables': {
    'variables': {
      # We have to nest variables inside variables as a hack for variables
      # override.

      # WARNING: build/features_override.gypi which is included in a full
      # chromium build, overrides this list with its own values. See
      # features_override.gypi inline documentation for more details.
      'feature_defines': [
        'ENABLE_WEBGL=1',
        'ENABLE_3D_RENDERING=1',
        'ENABLE_ACCELERATED_2D_CANVAS=1',
        'ENABLE_BLOB=1',
        'ENABLE_BLOB_SLICE=1',
        'ENABLE_CHANNEL_MESSAGING=1',
        'ENABLE_CLIENT_BASED_GEOLOCATION=1',
        'ENABLE_DASHBOARD_SUPPORT=0',
        'ENABLE_DATABASE=1',
        'ENABLE_DATAGRID=0',
        'ENABLE_DATA_TRANSFER_ITEMS=1',
        'ENABLE_DEVICE_ORIENTATION=1',
        'ENABLE_DIRECTORY_UPLOAD=1',
        'ENABLE_DOM_STORAGE=1',
        'ENABLE_EVENTSOURCE=1',
        'ENABLE_JAVASCRIPT_I18N_API=1',
        'ENABLE_FILE_SYSTEM=1',
        'ENABLE_FILTERS=1',
        'ENABLE_FULLSCREEN_API=1',
        'ENABLE_GEOLOCATION=1',
        'ENABLE_GESTURE_RECOGNIZER=1',
        'ENABLE_ICONDATABASE=0',
        'ENABLE_INDEXED_DATABASE=1',
        'ENABLE_INPUT_SPEECH=1',
        'ENABLE_JAVASCRIPT_DEBUGGER=1',
        'ENABLE_JSC_MULTIPLE_THREADS=0',
        'ENABLE_LEVELDB=1',
        'ENABLE_LINK_PREFETCH=1',
        'ENABLE_MATHML=0',
        'ENABLE_MEDIA_STATISTICS=1',
        'ENABLE_MEDIA_STREAM=1',
        'ENABLE_METER_TAG=1',
        'ENABLE_NOTIFICATIONS=1',
        'ENABLE_OFFLINE_WEB_APPLICATIONS=1',
        'ENABLE_OPENTYPE_SANITIZER=1',
        'ENABLE_ORIENTATION_EVENTS=0',
        'ENABLE_PAGE_VISIBILITY_API=0',
        'ENABLE_PROGRESS_TAG=1',
        'ENABLE_QUOTA=1',
        'ENABLE_REGISTER_PROTOCOL_HANDLER=0',
        'ENABLE_REQUEST_ANIMATION_FRAME=1',
        'ENABLE_SHARED_WORKERS=1',
        'ENABLE_SVG=1',
        'ENABLE_SVG_ANIMATION=1',
        'ENABLE_SVG_AS_IMAGE=1',
        'ENABLE_SVG_FONTS=1',
        'ENABLE_SVG_FOREIGN_OBJECT=1',
        'ENABLE_SVG_USE=1',
        'ENABLE_TOUCH_EVENTS=1',
        'ENABLE_V8_SCRIPT_DEBUG_SERVER=1',
        'ENABLE_VIDEO=1',
        'ENABLE_WEB_AUDIO=0',
        'ENABLE_WEB_SOCKETS=1',
        'ENABLE_WEB_TIMING=1',
        'ENABLE_WORKERS=1',
        'ENABLE_XHR_RESPONSE_BLOB=1',
        'ENABLE_XPATH=1',
        'ENABLE_XSLT=1',
        'WTF_USE_ACCELERATED_COMPOSITING=1',
        'WTF_USE_WEBP=1',
        'WTF_USE_WEBKIT_IMAGE_DECODERS=1',
      ],

      'use_accelerated_compositing%': 1,
      'enable_svg%': 1,
    },

    'feature_defines%': '<(feature_defines)',
    'use_accelerated_compositing%': '<(use_accelerated_compositing)',
    'enable_svg%': '<(enable_svg)',
  },
}
