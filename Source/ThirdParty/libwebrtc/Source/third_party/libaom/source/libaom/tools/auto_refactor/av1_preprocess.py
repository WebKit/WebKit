# Copyright (c) 2021, Alliance for Open Media. All rights reserved.
#
# This source code is subject to the terms of the BSD 2 Clause License and
# the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
# was not distributed with this source code in the LICENSE file, you can
# obtain it at www.aomedia.org/license/software. If the Alliance for Open
# Media Patent License 1.0 was not distributed with this source code in the
# PATENTS file, you can obtain it at www.aomedia.org/license/patent.
#

import os
import sys


def is_code_file(filename):
  return filename.endswith(".c") or filename.endswith(".h")


def is_simd_file(filename):
  simd_keywords = [
      "avx2", "sse2", "sse3", "ssse3", "sse4", "dspr2", "neon", "msa", "simd",
      "x86"
  ]
  for keyword in simd_keywords:
    if filename.find(keyword) >= 0:
      return True
  return False


def get_code_file_list(path, exclude_file_set):
  code_file_list = []
  for cur_dir, sub_dir, file_list in os.walk(path):
    for filename in file_list:
      if is_code_file(filename) and not is_simd_file(
          filename) and filename not in exclude_file_set:
        file_path = os.path.join(cur_dir, filename)
        code_file_list.append(file_path)
  return code_file_list


def av1_exclude_file_set():
  exclude_file_set = {
      "cfl_ppc.c",
      "ppc_cpudetect.c",
  }
  return exclude_file_set


def get_av1_pp_command(fake_header_dir, code_file_list):
  pre_command = "gcc -w -nostdinc -E -I./ -I../ -I" + fake_header_dir + (" "
                                                                         "-D'ATTRIBUTE_PACKED='"
                                                                         " "
                                                                         "-D'__attribute__(x)='"
                                                                         " "
                                                                         "-D'__inline__='"
                                                                         " "
                                                                         "-D'float_t=float'"
                                                                         " "
                                                                         "-D'DECLARE_ALIGNED(n,"
                                                                         " typ,"
                                                                         " "
                                                                         "val)=typ"
                                                                         " val'"
                                                                         " "
                                                                         "-D'volatile='"
                                                                         " "
                                                                         "-D'AV1_K_MEANS_DIM=2'"
                                                                         " "
                                                                         "-D'AOM_FORCE_INLINE='"
                                                                         " "
                                                                         "-D'inline='"
                                                                         )
  return pre_command + " " + " ".join(code_file_list)


def modify_av1_rtcd(build_dir):
  av1_rtcd = os.path.join(build_dir, "config/av1_rtcd.h")
  fp = open(av1_rtcd)
  string = fp.read()
  fp.close()
  new_string = string.replace("#ifdef RTCD_C", "#if 0")
  fp = open(av1_rtcd, "w")
  fp.write(new_string)
  fp.close()


def preprocess_av1(aom_dir, build_dir, fake_header_dir):
  cur_dir = os.getcwd()
  output = os.path.join(cur_dir, "av1_pp.c")
  path_list = [
      os.path.join(aom_dir, "av1/encoder"),
      os.path.join(aom_dir, "av1/common")
  ]
  code_file_list = []
  for path in path_list:
    path = os.path.realpath(path)
    code_file_list.extend(get_code_file_list(path, av1_exclude_file_set()))
  modify_av1_rtcd(build_dir)
  cmd = get_av1_pp_command(fake_header_dir, code_file_list) + " >" + output
  os.chdir(build_dir)
  os.system(cmd)
  os.chdir(cur_dir)


if __name__ == "__main__":
  aom_dir = sys.argv[1]
  build_dir = sys.argv[2]
  fake_header_dir = sys.argv[3]
  preprocess_av1(aom_dir, build_dir, fake_header_dir)
