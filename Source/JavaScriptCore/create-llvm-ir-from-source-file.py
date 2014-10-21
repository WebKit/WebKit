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
