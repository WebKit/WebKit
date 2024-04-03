#!/bin/bash -eu
# Copyright 2020 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################################

# This script is meant to be run by the oss-fuzz infrastructure from the script
# https://github.com/google/oss-fuzz/blob/master/projects/libavif/build.sh
# It builds the different fuzz targets.

# To test changes to this file:
# - make changes and commit to your REPO
# - run:
#     git clone --depth=1 git@github.com:google/oss-fuzz.git
#     cd oss-fuzz
# - modify projects/libavif/Dockerfile to point to your REPO
# - run:
#     python3 infra/helper.py build_image libavif
#     # enter 'y' and wait for everything to be downloaded
# - run:
#     python3 infra/helper.py build_fuzzers --sanitizer address libavif
#     # wait for the tests to be built
# And then run the fuzzer locally, for example:
#     python3 infra/helper.py run_fuzzer libavif \
#     avif_fuzztest_enc_dec_incr@EncodeDecodeAvifFuzzTest.EncodeDecodeGridValid \
#     --sanitizer address

# Reset compile flags to build dav1d without fuzzer flags. The meson build system
# is problematic with sanitizer flags.
export ORIG_CFLAGS="$CFLAGS"
export ORIG_CXXFLAGS="$CXXFLAGS"
export CFLAGS=""
export CXXFLAGS=""

cd ext && bash dav1d.cmd && bash libyuv.cmd && cd ..

export CFLAGS=$ORIG_CFLAGS
export CXXFLAGS=$ORIG_CXXFLAGS

# Prepare remaining dependencies.
cd ext && bash aom.cmd && bash fuzztest.cmd && bash libjpeg.cmd && bash libsharpyuv.cmd &&
      bash zlibpng.cmd && cd ..

# build libavif
mkdir build
cd build
if [ "$FUZZING_ENGINE" == "libfuzzer" ]
then
  export CXXFLAGS="${CXXFLAGS} -DFUZZTEST_COMPATIBILITY_MODE"
  export EXTRA_CMAKE_FLAGS="-DAVIF_ENABLE_FUZZTEST=ON -DFUZZTEST_COMPATIBILITY_MODE=libfuzzer"
fi
cmake .. -G Ninja -DBUILD_SHARED_LIBS=OFF -DAVIF_CODEC_AOM=LOCAL -DAVIF_CODEC_DAV1D=LOCAL \
      -DAVIF_CODEC_AOM_DECODE=ON -DAVIF_CODEC_AOM_ENCODE=ON \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DAVIF_ENABLE_WERROR=OFF \
      -DAVIF_LOCAL_FUZZTEST=ON \
      -DAVIF_LOCAL_GTEST=OFF -DAVIF_LOCAL_JPEG=ON -DAVIF_LOCAL_LIBSHARPYUV=ON \
      -DAVIF_LIBYUV=LOCAL -DAVIF_LOCAL_ZLIBPNG=ON \
      -DAVIF_BUILD_TESTS=ON -DAVIF_ENABLE_GTEST=OFF -DAVIF_ENABLE_WERROR=ON \
      ${EXTRA_CMAKE_FLAGS}

ninja

# build decode fuzzer
$CXX $CXXFLAGS -std=c++11 -I../include \
    ../tests/oss-fuzz/avif_decode_fuzzer.cc -o $OUT/avif_decode_fuzzer \
    $LIB_FUZZING_ENGINE libavif.a ../ext/dav1d/build/src/libdav1d.a \
    ../ext/libyuv/build/libyuv.a ../ext/aom/build.libavif/libaom.a

# Restrict fuzztest tests to the only compatible fuzz engine: libfuzzer.
if [ "$FUZZING_ENGINE" == "libfuzzer" ]
then
  # build fuzztests
  # The following is taken from https://github.com/google/oss-fuzz/blob/31ac7244748ea7390015455fb034b1f4eda039d9/infra/base-images/base-builder/compile_fuzztests.sh#L59
  # Iterate the fuzz binaries and list each fuzz entrypoint in the binary. For
  # each entrypoint create a wrapper script that calls into the binaries the
  # given entrypoint as argument.
  # The scripts will be named:
  # {binary_name}@{fuzztest_entrypoint}
  FUZZ_TEST_BINARIES_OUT_PATHS=`ls ./tests/avif_fuzztest_*`
  echo "Fuzz binaries: $FUZZ_TEST_BINARIES_OUT_PATHS"
  for fuzz_main_file in $FUZZ_TEST_BINARIES_OUT_PATHS; do
    FUZZ_TESTS=$($fuzz_main_file --list_fuzz_tests | cut -d ' ' -f 4)
    cp -f ${fuzz_main_file} $OUT/
    fuzz_basename=$(basename $fuzz_main_file)
    chmod -x $OUT/$fuzz_basename
    for fuzz_entrypoint in $FUZZ_TESTS; do
      TARGET_FUZZER="${fuzz_basename}@$fuzz_entrypoint"
      # Write executer script
      echo "#!/bin/sh
# LLVMFuzzerTestOneInput for fuzzer detection.
this_dir=\$(dirname \"\$0\")
export TEST_DATA_DIRS=\$this_dir/corpus
chmod +x \$this_dir/$fuzz_basename
\$this_dir/$fuzz_basename --fuzz=$fuzz_entrypoint -- \$@
chmod -x \$this_dir/$fuzz_basename" > $OUT/$TARGET_FUZZER
      chmod +x $OUT/$TARGET_FUZZER
    done
  done
fi

# copy seed corpus for fuzztest tests
mkdir $OUT/corpus
unzip $SRC/avif_decode_seed_corpus.zip -d $OUT/corpus
cp $SRC/libavif/tests/data/* $OUT/corpus

# create a bigger seed corpus for avif_decode_fuzzer
zip -j $OUT/avif_decode_fuzzer_seed_corpus.zip $OUT/corpus/*
