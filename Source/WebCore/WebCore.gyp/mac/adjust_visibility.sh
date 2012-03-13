#!/bin/sh

#
# Copyright (C) 2009 Google Inc. All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
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
#

# usage: adjust_visibility.sh INPUT OUTPUT WORK_DIR
#
# Transforms a static library at INPUT by marking all of its symbols
# private_extern.  The output is placed at OUTPUT.  WORK_DIR is used as a
# scratch directory, which need not exist before this script is invoked,
# and which will be left behind when the script exits.

set -e

if [ $# -ne 3 ] ; then
  echo "usage: ${0} INPUT OUTPUT WORK_DIR" >& 2
  exit 1
fi

INPUT="${1}"
OUTPUT="${2}"
WORK_DIR="${3}"

# Start with a clean slate.
rm -f "${OUTPUT}"
rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}"

# ar doesn't operate on fat files.  Figure out what architectures are
# involved.
ARCHS=$(file "${INPUT}" | sed -Ene 's/^.*\(for architecture (.+)\):.*$/\1/p')
if [ -z "${ARCHS}" ] ; then
  ARCHS=self
fi

OUTPUT_NAME="output.a"

for ARCH in ${ARCHS} ; do
  if [ "${ARCH}" = "ppc" -o "${ARCH}" = "ppc64" ] ; then
    echo "Please |lipo -remove ppc -remove ppc64| from ${INPUT}."
    echo "Xcode 4.3's ld can no longer relink ppc files."
    exit 1
  fi
  # Get a thin version of fat input by running lipo.  If the input is already
  # thin, just copy it into place.  The extra copy isn't strictly necessary
  # but it simplifies the script.
  ARCH_DIR="${WORK_DIR}/${ARCH}"
  mkdir -p "${ARCH_DIR}"
  INPUT_NAME=input.a
  ARCH_INPUT="${ARCH_DIR}/${INPUT_NAME}"
  if [ "${ARCHS}" = "self" ] ; then
    cp "${INPUT}" "${ARCH_INPUT}"
  else
    lipo -thin "${ARCH}" "${INPUT}" -output "${ARCH_INPUT}"
  fi

  # Change directories to extract the archive to ensure correct pathnames.
  (cd "${ARCH_DIR}" && ar -x "${INPUT_NAME}")

  # libWebKitSystemInterfaceLeopard.a's cuDbUtils.o references a few symbols
  # that are not defined in any framework in OS X 10.5. If it's linked into a
  # libwebkit.dylib with -Wl,-all_load, linking to libwebkit.dylib will result
  # in these unresolved symbols:
  # __ZN8Security12KeychainCore6Schema22X509CrlSchemaIndexListE$non_lazy_ptr
  # __ZN8Security12KeychainCore6Schema23X509CrlSchemaIndexCountE$non_lazy_ptr
  # __ZN8Security12KeychainCore6Schema26X509CrlSchemaAttributeListE$non_lazy_ptr
  # __ZN8Security12KeychainCore6Schema27X509CrlSchemaAttributeCountE$non_lazy_ptr
  # Since nothing in cuDbUtils.o is needed, just remove it.
  rm "${ARCH_DIR}"/cuDbUtils.o

  # Use ld -r to relink each object that was in the archive.  Providing an
  # empty -exported_symbols_list will transform all symbols to private_extern;
  # these symbols are retained with -keep_private_externs.
  for OBJECT in "${ARCH_DIR}/"*.o ; do
    NEW_OBJECT="${OBJECT}.new"
    ld -o "${NEW_OBJECT}" -r "${OBJECT}" \
        -exported_symbols_list /dev/null -keep_private_externs
    mv "${NEW_OBJECT}" "${OBJECT}"
  done

  # Build an architecture-specific archive from the modified object files.
  ARCH_OUTPUT="${ARCH_DIR}/${OUTPUT_NAME}"
  (cd "${ARCH_DIR}" && ar -rc "${OUTPUT_NAME}" *.o)
  ranlib "${ARCH_OUTPUT}"

  # Toss the object files out now that they're in the archive.
  rm -f "${ARCH_DIR}/"*.o
done

# Create a fat archive from the architecture-specific archives if needed.
# If the input was thin, leave the output thin by copying the only output
# archive to the destination.
if [ "${ARCHS}" = "self" ] ; then
  cp "${WORK_DIR}/self/${OUTPUT_NAME}" "${OUTPUT}"
else
  lipo -create -output "${OUTPUT}" "${WORK_DIR}/"*"/${OUTPUT_NAME}"
fi
