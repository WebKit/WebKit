# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A wrapper to run yasm.

Its main job is to provide a Python wrapper for GN integration, and to write
the makefile-style output yasm generates in stdout to a .d file for dependency
management of .inc files.

Run with:
  python run_yasm.py <yasm_binary_path> <all other yasm args>

Note that <all other yasm args> must include an explicit output file (-o). This
script will append a ".d" to this and write the dependencies there. This script
will add "-M" to cause yasm to write the deps to stdout, so you don't need to
specify that.
"""

import argparse
import sys
import subprocess
import os

# Extract the output file name from the yasm command line so we can generate a
# .d file with the same base name.
parser = argparse.ArgumentParser()

yasm_base = os.path.dirname(os.path.realpath(__file__)) + "/"
vpx_base = yasm_base + "../"

libvpx_srcs_x86_64_assembly = [
  "source/libvpx/vp8/common/x86/copy_sse2.asm",
  "source/libvpx/vp8/common/x86/copy_sse3.asm",
  "source/libvpx/vp8/common/x86/dequantize_mmx.asm",
  "source/libvpx/vp8/common/x86/idctllm_mmx.asm",
  "source/libvpx/vp8/common/x86/idctllm_sse2.asm",
  "source/libvpx/vp8/common/x86/iwalsh_sse2.asm",
  "source/libvpx/vp8/common/x86/loopfilter_block_sse2_x86_64.asm",
  "source/libvpx/vp8/common/x86/loopfilter_sse2.asm",
  "source/libvpx/vp8/common/x86/mfqe_sse2.asm",
  "source/libvpx/vp8/common/x86/recon_mmx.asm",
  "source/libvpx/vp8/common/x86/recon_sse2.asm",
  "source/libvpx/vp8/common/x86/subpixel_mmx.asm",
  "source/libvpx/vp8/common/x86/subpixel_sse2.asm",
  "source/libvpx/vp8/common/x86/subpixel_ssse3.asm",
  "source/libvpx/vp8/encoder/x86/dct_sse2.asm",
  "source/libvpx/vp8/encoder/x86/encodeopt.asm",
  "source/libvpx/vp8/encoder/x86/fwalsh_sse2.asm",
  "source/libvpx/vp9/common/x86/vp9_mfqe_sse2.asm",
  "source/libvpx/vp9/encoder/x86/vp9_dct_sse2.asm",
  "source/libvpx/vp9/encoder/x86/vp9_error_sse2.asm",
  "source/libvpx/vp9/encoder/x86/vp9_quantize_ssse3_x86_64.asm",
  "source/libvpx/vpx_dsp/x86/add_noise_sse2.asm",
  "source/libvpx/vpx_dsp/x86/avg_ssse3_x86_64.asm",
  "source/libvpx/vpx_dsp/x86/deblock_sse2.asm",
  "source/libvpx/vpx_dsp/x86/fwd_txfm_ssse3_x86_64.asm",
  "source/libvpx/vpx_dsp/x86/highbd_intrapred_sse2.asm",
  "source/libvpx/vpx_dsp/x86/highbd_sad4d_sse2.asm",
  "source/libvpx/vpx_dsp/x86/highbd_sad_sse2.asm",
  "source/libvpx/vpx_dsp/x86/highbd_subpel_variance_impl_sse2.asm",
  "source/libvpx/vpx_dsp/x86/highbd_variance_impl_sse2.asm",
  "source/libvpx/vpx_dsp/x86/intrapred_sse2.asm",
  "source/libvpx/vpx_dsp/x86/intrapred_ssse3.asm",
  "source/libvpx/vpx_dsp/x86/inv_wht_sse2.asm",
  "source/libvpx/vpx_dsp/x86/sad4d_sse2.asm",
  "source/libvpx/vpx_dsp/x86/sad_sse2.asm",
  "source/libvpx/vpx_dsp/x86/sad_sse3.asm",
  "source/libvpx/vpx_dsp/x86/sad_sse4.asm",
  "source/libvpx/vpx_dsp/x86/sad_ssse3.asm",
  "source/libvpx/vpx_dsp/x86/ssim_opt_x86_64.asm",
  "source/libvpx/vpx_dsp/x86/subpel_variance_sse2.asm",
  "source/libvpx/vpx_dsp/x86/subtract_sse2.asm",
  "source/libvpx/vpx_dsp/x86/vpx_convolve_copy_sse2.asm",
  "source/libvpx/vpx_dsp/x86/vpx_high_subpixel_8t_sse2.asm",
  "source/libvpx/vpx_dsp/x86/vpx_high_subpixel_bilinear_sse2.asm",
  "source/libvpx/vpx_dsp/x86/vpx_subpixel_8t_sse2.asm",
  "source/libvpx/vpx_dsp/x86/vpx_subpixel_8t_ssse3.asm",
  "source/libvpx/vpx_dsp/x86/vpx_subpixel_bilinear_sse2.asm",
  "source/libvpx/vpx_dsp/x86/vpx_subpixel_bilinear_ssse3.asm",
  "source/libvpx/vpx_ports/emms.asm",
]

execution_arguments = [yasm_base + "yasm",
    "-fmacho64",
    #"-m x86",
     "-I", vpx_base + "source/config",
     "-I", vpx_base + "source/config/mac/x64",
     "-I", vpx_base + "source/libvpx"
     ]

# Assemble.
for filename in libvpx_srcs_x86_64_assembly:
    print filename
    base_filename = vpx_base + filename
    print execution_arguments + ["-o", base_filename + ".o", base_filename]
    result_code = subprocess.call(execution_arguments + ["-o", base_filename + ".o", base_filename])
    if result_code != 0:
        sys.exit(result_code)

# Now generate the .d file listing the dependencies. The -M option makes yasm
# write the Makefile-style dependencies to stdout, but it seems that inhibits
# generating any compiled output so we need to do this in a separate pass.
# However, outputting deps seems faster than actually assembling, and yasm is
# so fast anyway this is not a big deal.
#
# This guarantees proper dependency management for assembly files. Otherwise,
# we would have to require people to manually specify the .inc files they
# depend on in the build file, which will surely be wrong or out-of-date in
# some cases.

#deps = subprocess.check_output(execution_arguments + ['-M'])
#with open(depfile, "wb") as f:
#  f.write(deps)

