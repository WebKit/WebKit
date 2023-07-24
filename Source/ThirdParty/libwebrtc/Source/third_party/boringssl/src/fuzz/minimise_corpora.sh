#!/bin/bash
# Copyright (c) 2016, Google Inc.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
# OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

set -e

buildDir=../build
if [ $# -gt 0 ]; then
  buildDir=$1
fi

for testSource in $(ls -1 *.cc); do
  test=$(echo $testSource | sed -e 's/\.cc$//')
  if [ ! -x $buildDir/fuzz/$test ] ; then
    echo "Failed to find binary for $test"
    exit 1
  fi
  if [ ! -d ${test}_corpus ]; then
    echo "Failed to find corpus directory for $test"
    exit 1
  fi
  if [ -d ${test}_corpus_old ]; then
    echo "Old corpus directory for $test already exists"
    exit 1
  fi
done

for testSource in $(ls -1 *.cc); do
  test=$(echo $testSource | sed -e 's/\.cc$//')
  mv ${test}_corpus ${test}_corpus_old
  mkdir ${test}_corpus
  $buildDir/fuzz/$test -max_len=50000 -merge=1 ${test}_corpus ${test}_corpus_old
  rm -Rf ${test}_corpus_old
done
