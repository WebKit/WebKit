#!/usr/bin/env python
# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper around WebKitTools/Scripts/webkitpy/thirdparty/pywebsocket/mod_pywebsocket/standalone.py"""
import os
import sys

sys.path.append(os.path.join(os.path.dirname(os.path.abspath(sys.argv[0])),
                             "..", "Scripts", "webkitpy", "thirdparty",
                             "pywebsocket", "mod_pywebsocket"))
import standalone.py

if __name__ == '__main__':
    standalone._main()
