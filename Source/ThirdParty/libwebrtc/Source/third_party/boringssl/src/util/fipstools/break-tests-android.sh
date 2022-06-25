# Copyright (c) 2019, Google Inc.
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
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

# This script exists to exercise breaking each of the FIPS tests on an Android
# device. Since, on Android, BoringCrypto exists in both 32- and 64-bit
# versions, the first argument must be either "32" or "64" to select which is
# being tested. The Android source tree must have been setup (with "lunch") for
# a matching build configuration before using this script to build the
# binaries. (Although it'll fail non-silently if there's a mismatch.)
#
# Since each test needs the FIPS module to be compiled differently, and that
# can take a long time, this script is run twice: once with "build" as the
# second argument to run the builds, and then with "run" as the second argument
# to run each test.
#
# Run it with /bin/bash, not /bin/sh, otherwise "read" may fail.
#
# In order to reconfigure the build for each test, it needs to set a define. It
# does so by rewriting a template in external/boringssl/Android.bp and you must
# add the template value before doing the builds. To do so, insert
# -DBORINGSSL_FIPS_BREAK_XXX=1 in the cflags list for the module, probably by
# putting it in the "boringssl_flags" stanza.

set -x
set -e

if [ ! -f external/boringssl/Android.bp ]; then
  echo "Must be run from the top-level of an Android source tree."
  exit 1
fi

. build/envsetup.sh

TESTS="NONE ECDSA_PWCT CRNG RSA_PWCT AES_CBC AES_GCM DES SHA_1 SHA_256 SHA_512 RSA_SIG DRBG ECDSA_SIG Z_COMPUTATION TLS_KDF FFC_DH"

if [ "x$1" = "x32" ]; then
  lib="lib"
  bits="32"
elif [ "x$1" = "x64" ] ; then
  lib="lib64"
  bits="64"
else
  echo "First argument must be 32 or 64"
  exit 1
fi

if [ "x$2" = "xbuild" ]; then
  if ! grep -q DBORINGSSL_FIPS_BREAK_XXX=1 external/boringssl/Android.bp; then
    echo "Missing DBORINGSSL_FIPS_BREAK_XXX in external/boringssl/Android.bp. Edit the file and insert -DBORINGSSL_FIPS_BREAK_XXX=1 in the cflags for the FIPS module"
    exit 1
  fi

  printf "\\x1b[1mBuilding modules\\x1b[0m\n"
  for test in $TESTS; do
    printf "\\x1b[1mBuilding for ${test}\\x1b[0m\n"
    cp external/boringssl/Android.bp external/boringssl/Android.bp.orig
    sed -i -e "s/DBORINGSSL_FIPS_BREAK_XXX/DBORINGSSL_FIPS_BREAK_${test}/" external/boringssl/Android.bp
    m test_fips
    dir=test-${bits}-${test}
    rm -Rf $dir
    mkdir $dir
    cp ${ANDROID_PRODUCT_OUT}/system/${lib}/libcrypto.so $dir
    cp ${ANDROID_PRODUCT_OUT}/system/bin/test_fips $dir
    if [ $bits = "32" ] ; then
      if ! file ${dir}/test_fips | grep -q "32-bit" ; then
        echo "32-bit build requested but binaries don't appear to be 32-bit:"
        file ${dir}/test_fips
        exit 1
      fi
    else
      if ! file ${dir}/test_fips | grep -q "64-bit" ; then
        echo "64-bit build requested but binaries don't appear to be 64-bit:"
        file ${dir}/test_fips
        exit 1
      fi
    fi
    cp external/boringssl/Android.bp.orig external/boringssl/Android.bp
  done
elif [ "x$2" = "xrun" ]; then
  printf "\\x1b[1mTesting\\x1b[0m\n"
  for test in $TESTS; do
    dir=test-${bits}-${test}
    if [ ! '(' -d ${dir} -a -f ${dir}/test_fips -a -f ${dir}/libcrypto.so ')' ] ; then
      echo "Build directory ${dir} is missing or is missing files"
      exit 1
    fi
    adb push ${dir}/* /data/local/tmp
    printf "\\x1b[1mTesting ${test}\\x1b[0m\n"
    adb shell -n -t -x LD_LIBRARY_PATH=/data/local/tmp /data/local/tmp/test_fips
    read
  done

  printf "\\x1b[1mTesting integrity}\\x1b[0m\n"
  src=test-${bits}-NONE
  dir=test-${bits}-INT
  rm -Rf $dir
  mkdir $dir
  go run external/boringssl/src/util/fipstools/break-hash.go ${src}/libcrypto.so ${dir}/libcrypto.so
  cp ${src}/test_fips $dir
  adb push ${dir}/* /data/local/tmp
  adb shell -n -t -x LD_LIBRARY_PATH=/data/local/tmp /data/local/tmp/test_fips
  read
else
  echo "Second argument must be build or run"
  exit 1
fi
