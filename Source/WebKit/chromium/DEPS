#
# Copyright (C) 2009 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#   * Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright 
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#   * Neither the name of Google Inc. nor the names of its contributors 
#     may be used to endorse or promote products derived from this software
#     without specific prior written permission.
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

# This file is used by gclient to fetch the projects that the webkit
# chromium port depends on.

vars = {
  'chromium_svn': 'http://src.chromium.org/svn/trunk/src',
  'chromium_rev': '81564'
}

deps = {
  'chromium_deps':
    File(Var('chromium_svn')+'/DEPS@'+Var('chromium_rev')),

  # build tools
  'build':
    Var('chromium_svn')+'/build@'+Var('chromium_rev'),
  'tools/data_pack':
    Var('chromium_svn')+'/tools/data_pack@'+Var('chromium_rev'),
  'tools/gyp':
    From('chromium_deps', 'src/tools/gyp'),

  # Basic tools
  'base':
    Var('chromium_svn')+'/base@'+Var('chromium_rev'),

  # skia dependencies
  'skia':
    Var('chromium_svn')+'/skia@'+Var('chromium_rev'),
  'third_party/skia/gpu':
    From('chromium_deps', 'src/third_party/skia/gpu'),
  'third_party/skia/src':
    From('chromium_deps', 'src/third_party/skia/src'),
  'third_party/skia/include':
    From('chromium_deps', 'src/third_party/skia/include'),

  # testing
  'testing':
    Var('chromium_svn')+'/testing@'+Var('chromium_rev'),
  'testing/gtest':
    From('chromium_deps', 'src/testing/gtest'),
  'testing/gmock':
    From('chromium_deps', 'src/testing/gmock'),

  # v8 javascript engine
  'v8': From('chromium_deps', 'src/v8'),

  # net dependencies
  'net':
    Var('chromium_svn')+'/net@'+Var('chromium_rev'),
  'sdch':
    Var('chromium_svn')+'/sdch@'+Var('chromium_rev'),
  'sdch/open-vcdiff':
    From('chromium_deps', 'src/sdch/open-vcdiff'),
  'googleurl':
    From('chromium_deps', 'src/googleurl'),

  # webkit dependencies
  'webkit': Var('chromium_svn')+'/webkit@'+Var('chromium_rev'),

  'app':
    Var('chromium_svn')+'/app@'+Var('chromium_rev'), # needed by appcache
  'gpu':
    Var('chromium_svn')+'/gpu@'+Var('chromium_rev'),
  'ipc':
    Var('chromium_svn')+'/ipc@'+Var('chromium_rev'),
  'media':
    Var('chromium_svn')+'/media@'+Var('chromium_rev'),
  'printing':
    Var('chromium_svn')+'/printing@'+Var('chromium_rev'),
  'ppapi':
    Var('chromium_svn')+'/ppapi@'+Var('chromium_rev'),
  'third_party/angle':  # needed by the gpu process
    From('chromium_deps', 'src/third_party/angle'),
  'third_party/libvpx': # needed by webkit/media
    From('chromium_deps', 'src/third_party/libvpx'),
  'third_party/ffmpeg': # needed by webkit/media
    From('chromium_deps', 'src/third_party/ffmpeg'),
  'third_party/libjingle/source':
    From('chromium_deps', 'src/third_party/libjingle/source'),
  'third_party/libwebp':
    Var('chromium_svn')+'/third_party/libwebp@'+Var('chromium_rev'),
  'tools/grit':
    Var('chromium_svn')+'/tools/grit@'+Var('chromium_rev'),
  'tools/generate_stubs':
    Var('chromium_svn')+'/tools/generate_stubs@'+Var('chromium_rev'),
  'ui':
    Var('chromium_svn')+'/ui@'+Var('chromium_rev'), # needed by app

  # other third party
  'third_party/icu':
    From('chromium_deps', 'src/third_party/icu'),
  'third_party/ots':
    From('chromium_deps', 'src/third_party/ots'),
  'third_party/yasm/source/patched-yasm':
    From('chromium_deps', 'src/third_party/yasm/source/patched-yasm'),
  'third_party/libjpeg_turbo':
    From('chromium_deps', 'src/third_party/libjpeg_turbo'),
  'third_party/leveldb':
    From('chromium_deps', 'src/third_party/leveldb'),
  'third_party/snappy/src':
    From('chromium_deps', 'src/third_party/snappy/src'),
  'third_party':
    Var('chromium_svn')+'/third_party@'+Var('chromium_rev'),
}

deps_os = {
  'win': {
    'third_party/cygwin':
      From('chromium_deps', 'src/third_party/cygwin'),
    'third_party/ffmpeg/binaries/chromium/win/ia32':
      From('chromium_deps', 'src/third_party/ffmpeg/binaries/chromium/win/ia32'),
    'third_party/lighttpd':
      From('chromium_deps', 'src/third_party/lighttpd'),
    'third_party/nss':
      From('chromium_deps', 'src/third_party/nss'),
    # Dependencies used by libjpeg-turbo
    'third_party/yasm/binaries':
      From('chromium_deps', 'src/third_party/yasm/binaries'),
  },
  'mac': {
    'third_party/nss':
      From('chromium_deps', 'src/third_party/nss'),
  },
  'unix': {
    # Linux, actually.
    'tools/xdisplaycheck':
      Var('chromium_svn')+'/tools/xdisplaycheck@'+Var('chromium_rev'),
    'third_party/openssl':
      From('chromium_deps', 'src/third_party/openssl'),
  },
}

skip_child_includes = [
   # Don't look for dependencies in the following folders: 
   'base',
   'build',
   'googleurl',
   'net',
   'sdch',
   'skia',
   'testing',
   'third_party',
   'tools',
   'v8',
   'webkit',
]

include_rules = [
  # Everybody can use some things.
  '+base',
  '+build',
  '+ipc',

  # For now, we allow ICU to be included by specifying 'unicode/...', although
  # this should probably change.
  '+unicode',
  '+testing',

  # Allow anybody to include files from the 'public' Skia directory in the
  # webkit port. This is shared between the webkit port and Chromium.
  '+webkit/port/platform/graphics/skia/public',
]


hooks = [
  {
    # A change to any file in this directory should run the gyp generator.
    'pattern': '.',
    'action': ['python', 'gyp_webkit'],
  },
]
