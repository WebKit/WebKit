#!/bin/bash
# Copyright (c) 2010 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generates GLSL ES lexer - glslang_lex.cpp

script_dir=$(dirname $0)
input_file=$script_dir/glslang.l
output_file=$script_dir/glslang_lex.cpp
flex --noline --nounistd --outfile=$output_file $input_file
