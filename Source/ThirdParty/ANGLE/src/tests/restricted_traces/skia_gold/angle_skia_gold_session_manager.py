# Copyright 2020 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""ANGLE implementation of //build/skia_gold_common/skia_gold_session_manager.py."""

import os
import sys

d = os.path.dirname
THIS_DIR = d(os.path.abspath(__file__))
ANGLE_SRC_DIR = d(d(d(d(THIS_DIR))))
sys.path.insert(0, os.path.join(ANGLE_SRC_DIR, 'build'))
CHROMIUM_SRC_DIR = d(d(ANGLE_SRC_DIR))
sys.path.insert(0, os.path.join(CHROMIUM_SRC_DIR, 'build'))

from skia_gold_common import skia_gold_session_manager as sgsm
import angle_skia_gold_session


class ANGLESkiaGoldSessionManager(sgsm.SkiaGoldSessionManager):

    @staticmethod
    def _GetDefaultInstance():
        return 'angle'

    @staticmethod
    def GetSessionClass():
        return angle_skia_gold_session.ANGLESkiaGoldSession
