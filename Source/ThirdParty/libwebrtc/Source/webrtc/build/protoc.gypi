# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# Copied from Chromium's src/build/protoc.gypi
#
# It was necessary to copy this file to WebRTC, because the path to
# build/common.gypi is different for the standalone and Chromium builds. Gyp
# doesn't permit conditional inclusion or variable expansion in include paths.
# http://code.google.com/p/gyp/wiki/InputFormatReference#Including_Other_Files
#
# Local changes:
# * Removed <(DEPTH) from include_dir due to difficulties with generated
#   downstream code.


# This file is meant to be included into a target to provide a rule
# to invoke protoc in a consistent manner. For Java-targets, see
# protoc_java.gypi.
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'my_proto_lib',
#   'type': 'static_library',
#   'sources': [
#     'foo.proto',
#     'bar.proto',
#   ],
#   'variables': {
#     # Optional, see below: 'proto_in_dir': '.'
#     'proto_out_dir': 'dir/for/my_proto_lib'
#   },
#   'includes': ['path/to/this/gypi/file'],
# }
# If necessary, you may add normal .cc files to the sources list or other gyp
# dependencies.  The proto headers are guaranteed to be generated before any
# source files, even within this target, are compiled.
#
# The 'proto_in_dir' variable must be the relative path to the
# directory containing the .proto files.  If left out, it defaults to '.'.
#
# The 'proto_out_dir' variable specifies the path suffix that output
# files are generated under.  Targets that gyp-depend on my_proto_lib
# will be able to include the resulting proto headers with an include
# like:
#   #include "dir/for/my_proto_lib/foo.pb.h"
#
# If you need to add an EXPORT macro to a protobuf's c++ header, set the
# 'cc_generator_options' variable with the value: 'dllexport_decl=FOO_EXPORT:'
# e.g. 'dllexport_decl=BASE_EXPORT:'
#
# It is likely you also need to #include a file for the above EXPORT macro to
# work. You can do so with the 'cc_include' variable.
# e.g. 'base/base_export.h'
#
# Implementation notes:
# A proto_out_dir of foo/bar produces
#   <(SHARED_INTERMEDIATE_DIR)/protoc_out/foo/bar/{file1,file2}.pb.{cc,h}
#   <(SHARED_INTERMEDIATE_DIR)/pyproto/foo/bar/{file1,file2}_pb2.py

{
  'variables': {
    'protoc_wrapper': '<(DEPTH)/tools/protoc_wrapper/protoc_wrapper.py',
    'cc_dir': '<(SHARED_INTERMEDIATE_DIR)/protoc_out/<(proto_out_dir)',
    'py_dir': '<(PRODUCT_DIR)/pyproto/<(proto_out_dir)',
    'cc_generator_options%': '',
    'cc_include%': '',
    'proto_in_dir%': '.',
    'conditions': [
      ['use_system_protobuf==0', {
        'protoc': '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
      }, { # use_system_protobuf==1
        'protoc': '<!(which protoc)',
      }],
    ],
  },
  'rules': [
    {
      'rule_name': 'genproto',
      'extension': 'proto',
      'inputs': [
        '<(protoc_wrapper)',
        '<(protoc)',
      ],
      'outputs': [
        '<(py_dir)/<(RULE_INPUT_ROOT)_pb2.py',
        '<(cc_dir)/<(RULE_INPUT_ROOT).pb.cc',
        '<(cc_dir)/<(RULE_INPUT_ROOT).pb.h',
      ],
      'action': [
        'python',
        '<(protoc_wrapper)',
        '--protoc',
        '<(protoc)',
        # Using the --arg val form (instead of --arg=val) allows gyp's msvs rule
        # generation to correct 'val' which is a path.
        '--proto-in-dir','<(proto_in_dir)',
        # Naively you'd use <(RULE_INPUT_PATH) here, but protoc requires
        # --proto_path is a strict prefix of the path given as an argument.
        '--cc-out-dir', '<(cc_generator_options)<(cc_dir)',
        '--py-out-dir', '<(py_dir)',
        '<(RULE_INPUT_ROOT)<(RULE_INPUT_EXT)',
      ],
      'message': 'Generating C++ and Python code from <(RULE_INPUT_PATH)',
      'process_outputs_as_sources': 1,
    },
  ],
  'include_dirs': [
    '<(SHARED_INTERMEDIATE_DIR)/protoc_out',
  ],
  'direct_dependent_settings': {
    'include_dirs': [
      '<(SHARED_INTERMEDIATE_DIR)/protoc_out',
    ]
  },
  # This target exports a hard dependency because it generates header
  # files.
  'hard_dependency': 1,
  'conditions': [
    ['build_protobuf==1', {
      'dependencies': [
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protoc#host',
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
      ],
      'export_dependent_settings': [
        # The generated headers reference headers within protobuf_lite,
        # so dependencies must be able to find those headers too.
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
      ],
    }],
  ],
}
