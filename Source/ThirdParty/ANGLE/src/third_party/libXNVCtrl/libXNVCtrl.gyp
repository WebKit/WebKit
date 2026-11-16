# Copyright 2016 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
    'targets':
    [{
        'target_name': 'libXNVCtrl',
        'type': 'static_library',
        'sources':
        [
            'src/NVCtrl.c',
            'src/NVCtrl.h',
            'src/NVCtrlLib.h',
            'src/nv_control.h',
        ],
    }],
}
