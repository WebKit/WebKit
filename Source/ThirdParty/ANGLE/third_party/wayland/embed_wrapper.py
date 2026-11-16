#!/usr/bin/env python3
# Copyright 2026 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Captures stdout of third_party/wayland/src/src/embed.py to a file.

embed.py emits a C const-array declaration on stdout; upstream's Meson build
captures it via `capture: true`. GN's action() requires the script to write to
a file path passed as an argument, so this wrapper redirects.
"""

import subprocess
import sys

embed_py, input_file, ident, output_file = sys.argv[1:5]
result = subprocess.run([sys.executable, embed_py, input_file, ident],
                        capture_output=True,
                        check=True)
with open(output_file, 'wb') as f:
    f.write(result.stdout)
