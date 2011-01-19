#!/bin/bash
# Copyright (c) 2010 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generates GLSL ES parser - glslang_tab.h and glslang_tab.cpp

script_dir=$(dirname $0)
input_file=$script_dir/glslang.y
output_header=$script_dir/glslang_tab.h
output_source=$script_dir/glslang_tab.cpp
bison --no-lines --skeleton=yacc.c --defines=$output_header --output=$output_source $input_file
