# Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This .gypi describes all of the sources and dependencies to build a
# unified "angle_white_box_tests" target, which contains all of the
# tests that exercise the ANGLE implementation. It requires a parent
# target to include this gypi in an executable target containing a
# gtest harness in a main.cpp.

{
    # Everything below this is duplicated in the GN build.
    # If you change anything also change angle/src/tests/BUILD.gn
    'variables':
    {
        'angle_white_box_tests_sources':
        [
            '<(angle_path)/src/tests/util_tests/PrintSystemInfoTest.cpp',
            '<(angle_path)/src/tests/test_utils/angle_test_configs.cpp',
            '<(angle_path)/src/tests/test_utils/angle_test_configs.h',
            '<(angle_path)/src/tests/test_utils/angle_test_instantiate.cpp',
            '<(angle_path)/src/tests/test_utils/angle_test_instantiate.h',
            '<(angle_path)/src/tests/test_utils/ANGLETest.cpp',
            '<(angle_path)/src/tests/test_utils/ANGLETest.h',
            '<(angle_path)/src/tests/test_utils/gl_raii.h',
        ],
        'angle_white_box_tests_win_sources':
        [
            '<(angle_path)/src/tests/gl_tests/D3D11EmulatedIndexedBufferTest.cpp',
            '<(angle_path)/src/tests/gl_tests/D3D11FormatTablesTest.cpp',
            '<(angle_path)/src/tests/gl_tests/D3D11InputLayoutCacheTest.cpp',
            '<(angle_path)/src/tests/gl_tests/D3DTextureTest.cpp',
            '<(angle_path)/src/tests/gl_tests/ErrorMessages.cpp'
        ],
    },
    'dependencies':
    [
        '<(angle_path)/src/angle.gyp:angle_common',
        '<(angle_path)/src/angle.gyp:libANGLE',
        '<(angle_path)/src/angle.gyp:libGLESv2_static',
        '<(angle_path)/src/angle.gyp:libEGL_static',
        '<(angle_path)/src/tests/tests.gyp:angle_test_support',
        '<(angle_path)/util/util.gyp:angle_util_static',
    ],
    'include_dirs':
    [
        '<(angle_path)/include',
        '<(angle_path)/src/tests'
    ],
    'sources':
    [
        '<@(angle_white_box_tests_sources)',
    ],
    'conditions':
    [
        ['OS=="win"',
        {
            'sources':
            [
                '<@(angle_white_box_tests_win_sources)',
            ],
        }],
    ]
}
