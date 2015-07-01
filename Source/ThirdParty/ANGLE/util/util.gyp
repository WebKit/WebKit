# Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
    'conditions':
    [
        ['OS=="win"',
        {
            'targets':
            [
                {
                    'target_name': 'angle_util',
                    'type': 'static_library',
                    'includes': [ '../build/common_defines.gypi', ],
                    'dependencies':
                    [
                        '<(angle_path)/src/angle.gyp:angle_common',
                        '<(angle_path)/src/angle.gyp:libEGL',
                        '<(angle_path)/src/angle.gyp:libGLESv2',
                    ],
                    'export_dependent_settings':
                    [
                        '<(angle_path)/src/angle.gyp:angle_common',
                    ],
                    'include_dirs':
                    [
                        '<(angle_path)/include',
                        '<(angle_path)/util',
                    ],
                    'sources':
                    [
                        'com_utils.h',
                        'keyboard.h',
                        'mouse.h',
                        'path_utils.h',
                        'random_utils.cpp',
                        'random_utils.h',
                        'shader_utils.cpp',
                        'shader_utils.h',
                        'testfixturetypes.h',
                        'EGLWindow.cpp',
                        'EGLWindow.h',
                        'Event.h',
                        'OSWindow.cpp',
                        'OSWindow.h',
                        'Timer.h',
                        'win32/Win32_path_utils.cpp',
                        'win32/Win32Timer.cpp',
                        'win32/Win32Timer.h',
                        'win32/Win32Window.cpp',
                        'win32/Win32Window.h',
                    ],
                    'msvs_disabled_warnings': [ 4201 ],
                    'direct_dependent_settings':
                    {
                        'include_dirs':
                        [
                            '<(angle_path)/include',
                            '<(angle_path)/util',
                        ],
                    },
                },
            ],
        }],
    ],
}
