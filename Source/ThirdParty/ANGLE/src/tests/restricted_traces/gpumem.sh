#!/bin/bash
#
# Copyright 2021 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
while true; do
    pid=$(pidof com.android.angle.test:test_process)
    case $pid in
        ''|*[!0-9]*) echo pid is not a number ;;
        *) echo com.android.angle.test:test_process $pid >> /sdcard/Download/gpumem.txt ;;
    esac
    dumpsys gpu --gpumem >> /sdcard/Download/gpumem.txt
    sleep 1;
done
