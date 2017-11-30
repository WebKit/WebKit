#!/bin/bash
# Copyright (c) 2010 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generates GLSL ES parser - glslang_lex.cpp, glslang_tab.h, and glslang_tab.cpp

run_flex()
{
input_file=./$1.l
output_source=./$1_lex.cpp
flex --noline --nounistd --outfile=$output_source $input_file
}

run_bison()
{
input_file=./$1.y
output_header=./$1_tab.h
output_source=./$1_tab.cpp
bison --no-lines --skeleton=yacc.c --defines=$output_header --output=$output_source $input_file
}

script_dir=$(dirname $0)

# Generate Parser
cd $script_dir
run_flex glslang
run_bison glslang
patch --silent --forward < 64bit-lexer-safety.patch
