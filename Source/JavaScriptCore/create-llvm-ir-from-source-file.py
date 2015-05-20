#!/usr/bin/python

# Copyright (C) 2014 University of Szeged. All rights reserved.
# Copyright (C) 2014 Samsung Electronics. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
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

import os
import subprocess
import sys

JSC_SOURCE = sys.argv[1]
RUNTIME_INSTALL_DIR = sys.argv[2]
CLANG_EXE = sys.argv[3]
INCLUDE_DIRS = "-I%s" % sys.argv[4]

try:
    os.mkdir(os.path.join(RUNTIME_INSTALL_DIR, "runtime"))
except OSError:
    pass

subprocess.call([CLANG_EXE, "-emit-llvm", "-O3", "-std=c++11", "-fno-exceptions", "-fno-strict-aliasing", "-fno-rtti", "-ffunction-sections", "-fdata-sections", "-fno-rtti", "-fno-omit-frame-pointer", "-fPIC", "-DWTF_PLATFORM_EFL=1", "-o", os.path.join(RUNTIME_INSTALL_DIR, os.path.splitext(JSC_SOURCE)[0] + ".bc"), "-c", JSC_SOURCE] + INCLUDE_DIRS.split())
