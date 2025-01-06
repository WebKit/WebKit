#!/bin/sh
## Copyright (c) 2023, Alliance for Open Media. All rights reserved.
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
##  This script checks the bit exactness between C and SIMD
##  implementations of AV1 encoder.
##
. $(dirname $0)/tools_common.sh

PRESETS="good rt"
LOWBD_CIF_CLIP="yuv_raw_input"
LOWBD_480p_CLIP="yuv_480p_raw_input"
LOWBD_720p_CLIP="y4m_720p_input"
HIGHBD_CLIP="y4m_360p_10bit_input"
SC_CLIP="y4m_screen_input"
OUT_FILE_SUFFIX=".ivf"
SCRIPT_DIR=$(dirname "$0")
LIBAOM_SOURCE_DIR=$(cd ${SCRIPT_DIR}/..; pwd)

# Clips used in test.
YUV_RAW_INPUT="${LIBAOM_TEST_DATA_PATH}/hantro_collage_w352h288.yuv"
YUV_480P_RAW_INPUT="${LIBAOM_TEST_DATA_PATH}/niklas_640_480_30.yuv"
Y4M_360P_10BIT_INPUT="${LIBAOM_TEST_DATA_PATH}/crowd_run_360p_10_150f.y4m"
Y4M_720P_INPUT="${LIBAOM_TEST_DATA_PATH}/niklas_1280_720_30.y4m"
Y4M_SCREEN_INPUT="${LIBAOM_TEST_DATA_PATH}/wikipedia_420_360p_60f.y4m"

# Number of frames to test.
AV1_ENCODE_C_VS_SIMD_TEST_FRAME_LIMIT=35

# Create a temporary directory for output files.
if [ -n "${TMPDIR}" ]; then
  AOM_TEST_TEMP_ROOT="${TMPDIR}"
elif [ -n "${TEMPDIR}" ]; then
  AOM_TEST_TEMP_ROOT="${TEMPDIR}"
else
  AOM_TEST_TEMP_ROOT=/tmp
fi

AOM_TEST_OUTPUT_DIR="${AOM_TEST_TEMP_ROOT}/av1_test_$$"

if ! mkdir -p "${AOM_TEST_OUTPUT_DIR}" || \
   [ ! -d "${AOM_TEST_OUTPUT_DIR}" ]; then
  echo "${0##*/}: Cannot create output directory, giving up."
  echo "${0##*/}:   AOM_TEST_OUTPUT_DIR=${AOM_TEST_OUTPUT_DIR}"
  exit 1
fi

elog() {
  echo "$@" 1>&2
}

# Echoes path to $1 when it's executable and exists in ${AOM_TEST_OUTPUT_DIR},
# or an empty string. Caller is responsible for testing the string once the
# function returns.
av1_enc_tool_path() {
  local target="$1"
  local preset="$2"
  local tool_path="${AOM_TEST_OUTPUT_DIR}/build_target_${target}/aomenc_${preset}"

  if [ ! -x "${tool_path}" ]; then
    tool_path=""
  fi
  echo "${tool_path}"
}

# Environment check: Make sure input and source directories are available.
av1_c_vs_simd_enc_verify_environment () {
  if [ ! -e "${YUV_RAW_INPUT}" ]; then
    elog "libaom test data must exist in LIBAOM_TEST_DATA_PATH."
    return 1
  fi
  if [ ! -e "${Y4M_360P_10BIT_INPUT}" ]; then
    elog "libaom test data must exist in LIBAOM_TEST_DATA_PATH."
    return 1
  fi
  if [ ! -e "${YUV_480P_RAW_INPUT}" ]; then
    elog "libaom test data must exist in LIBAOM_TEST_DATA_PATH."
    return 1
  fi
  if [ ! -e "${Y4M_720P_INPUT}" ]; then
    elog "libaom test data must exist in LIBAOM_TEST_DATA_PATH."
    return 1
  fi
  if [ ! -e "${Y4M_SCREEN_INPUT}" ]; then
    elog "libaom test data must exist in LIBAOM_TEST_DATA_PATH."
    return 1
  fi
  if [ ! -d "$LIBAOM_SOURCE_DIR" ]; then
    elog "LIBAOM_SOURCE_DIR does not exist."
    return 1
  fi
}

# This is not needed since tools_common.sh does the same cleanup.
# Keep the code here for our reference.
# cleanup() {
#  rm -rf  ${AOM_TEST_OUTPUT_DIR}
# }

# Echo AOM_SIMD_CAPS_MASK for different instruction set architecture.
avx2() {
   echo "0x1FF"
}

avx() {
   echo "0x17F"
}

sse4_2() {
   echo "0x13F"
}

sse4_1() {
   echo "0x03F"
}

ssse3() {
   echo "0x01F"
}

sse3() {
   echo "0x00F"
}

sse2() {
   echo "0x007"
}

get_bitrates() {
  local content=$1
  local preset=$2

  # Bit-rates:
  local bitrate_lowres_good="300"
  local bitrate_480p_good="500"
  local bitrate_720p_good="1000"
  local bitrate_scc_360p_good="500"
  local bitrate_lowres_rt="200"
  local bitrate_480p_rt="300"
  local bitrate_720p_rt="600"
  local bitrate_scc_360p_rt="300"
  local bitrate_hbd_360p="500"

  if [ "${preset}" = "good" ]; then
    if [ "${content}" = "yuv_raw_input" ]; then
      echo "${bitrate_lowres_good}"
    elif [ "${content}" = "yuv_480p_raw_input" ]; then
      echo "${bitrate_480p_good}"
    elif [ "${content}" = "y4m_720p_input" ]; then
      echo "${bitrate_720p_good}"
    elif [ "${content}" = "y4m_screen_input" ]; then
      echo "${bitrate_scc_360p_good}"
    elif [ "${content}" = "y4m_360p_10bit_input" ]; then
      echo "${bitrate_hbd_360p}"
    else
      elog "Invalid content"
    fi
  elif  [ "${preset}" = "rt" ]; then
    if [ "${content}" = "yuv_raw_input" ]; then
      echo "${bitrate_lowres_rt}"
    elif [ "${content}" = "yuv_480p_raw_input" ]; then
      echo "${bitrate_480p_rt}"
    elif [ "${content}" = "y4m_720p_input" ]; then
      echo "${bitrate_720p_rt}"
    elif [ "${content}" = "y4m_screen_input" ]; then
      echo "${bitrate_scc_360p_rt}"
    elif [ "${content}" = "y4m_360p_10bit_input" ]; then
      echo "${bitrate_hbd_360p}"
    else
      elog "Invalid content"
    fi
  else
    elog "invalid preset"
  fi
}

# Echo clip details to be used as input to aomenc.
yuv_raw_input() {
  echo ""${YUV_RAW_INPUT}"
       --width=352
       --height=288
       --bit-depth=8"
}

y4m_360p_10bit_input() {
  echo ""${Y4M_360P_10BIT_INPUT}"
       --bit-depth=10"
}

yuv_480p_raw_input() {
  echo ""${YUV_480P_RAW_INPUT}"
       --width=640
       --height=480
       --bit-depth=8"
}

y4m_720p_input() {
  echo ""${Y4M_720P_INPUT}"
       --bit-depth=8"
}

y4m_screen_input() {
  echo ""${Y4M_SCREEN_INPUT}"
       --tune-content=screen
       --enable-palette=1
       --bit-depth=8"
}

has_x86_isa_extn() {
  instruction_set=$1
  if ! grep -q "$instruction_set" /proc/cpuinfo; then
    # This instruction set is not supported.
    return 1
  fi
}

# Echo good encode params for use with AV1 encoder.
av1_encode_good_params() {
  echo "--good \
  --ivf \
  --profile=0 \
  --static-thresh=0 \
  --threads=1 \
  --tile-columns=0 \
  --tile-rows=0 \
  --verbose \
  --end-usage=vbr \
  --kf-max-dist=160 \
  --kf-min-dist=0 \
  --max-q=63 \
  --min-q=0 \
  --overshoot-pct=100 \
  --undershoot-pct=100 \
  --passes=2 \
  --arnr-maxframes=7 \
  --arnr-strength=5 \
  --auto-alt-ref=1 \
  --drop-frame=0 \
  --frame-parallel=0 \
  --lag-in-frames=35 \
  --maxsection-pct=2000 \
  --minsection-pct=0 \
  --sharpness=0"
}

# Echo realtime encode params for use with AV1 encoder.
av1_encode_rt_params() {
  echo "--rt \
  --ivf \
  --profile=0 \
  --static-thresh=0 \
  --threads=1 \
  --tile-columns=0 \
  --tile-rows=0 \
  --verbose \
  --end-usage=cbr \
  --kf-max-dist=90000 \
  --max-q=58 \
  --min-q=2 \
  --overshoot-pct=50 \
  --undershoot-pct=50 \
  --passes=1 \
  --aq-mode=3 \
  --buf-initial-sz=500 \
  --buf-optimal-sz=600 \
  --buf-sz=1000 \
  --coeff-cost-upd-freq=3 \
  --dv-cost-upd-freq=3 \
  --mode-cost-upd-freq=3 \
  --mv-cost-upd-freq=3 \
  --deltaq-mode=0 \
  --enable-global-motion=0 \
  --enable-obmc=0 \
  --enable-order-hint=0 \
  --enable-ref-frame-mvs=0 \
  --enable-tpl-model=0 \
  --enable-warped-motion=0 \
  --lag-in-frames=0 \
  --max-intra-rate=300 \
  --noise-sensitivity=0"
}

# Configures for the given target in AOM_TEST_OUTPUT_DIR/build_target_${target}
# directory.
av1_enc_build() {
  local target="$1"
  local cmake_command="$2"
  local tmp_build_dir=${AOM_TEST_OUTPUT_DIR}/build_target_${target}
  if [ -d "$tmp_build_dir" ]; then
    rm -rf $tmp_build_dir
  fi

  mkdir -p $tmp_build_dir
  cd $tmp_build_dir

  local cmake_common_args="--fresh -DCONFIG_EXCLUDE_SIMD_MISMATCH=1 \
           -DCMAKE_BUILD_TYPE=Release \
           -DENABLE_CCACHE=1 \
           '-DCMAKE_C_FLAGS_RELEASE=-O3 -g' \
           '-DCMAKE_CXX_FLAGS_RELEASE=-O3 -g' \
           -DENABLE_DOCS=0 -DENABLE_TESTS=0 -DENABLE_TOOLS=0"

  for preset in $PRESETS; do
    echo "Building target[${preset} encoding]: ${target}"
    if [ "${preset}" = "good" ]; then
      local cmake_extra_args="-DCONFIG_AV1_HIGHBITDEPTH=1"
    elif [ "${preset}" = "rt" ]; then
      local cmake_extra_args="-DCONFIG_REALTIME_ONLY=1 -DCONFIG_AV1_HIGHBITDEPTH=0"
    else
      elog "Invalid preset"
      return 1
    fi
    if ! eval "$cmake_command" "${cmake_common_args}" "${cmake_extra_args}" \
      ${devnull}; then
      elog "cmake failure"
      return 1
    fi
    if ! eval make -j$(nproc) aomenc ${devnull}; then
      elog "build failure"
      return 1
    fi

    mv aomenc aomenc_${preset}
  done
  echo "Done building target: ${target}"
}

compare_enc_output() {
  local target=$1
  local cpu=$2
  local clip=$3
  local bitrate=$4
  local preset=$5
  if ! diff -q ${AOM_TEST_OUTPUT_DIR}/Out-generic-"${clip}"-${preset}-${bitrate}kbps-cpu${cpu}${OUT_FILE_SUFFIX} \
       ${AOM_TEST_OUTPUT_DIR}/Out-${target}-"${clip}"-${preset}-${bitrate}kbps-cpu${cpu}${OUT_FILE_SUFFIX}; then
    elog "C vs ${target} encode mismatches for ${clip}, at ${bitrate} kbps, speed ${cpu}, ${preset} preset"
    return 1
  fi
}

av1_enc_test() {
  local encoder="$1"
  local arch="$2"
  local target="$3"
  local preset="$4"
  if [ -z "$(av1_enc_tool_path "${target}"  "${preset}")" ]; then
    elog "aomenc_${preset} not found. It must exist in ${AOM_TEST_OUTPUT_DIR}/build_target_${target} path"
    return 1
  fi

  if [ "${preset}" = "good" ]; then
    case "${arch}" in
      arm64)
        # Speed 0 is not tested as arm64 is run under emulation.
        local min_cpu_used=1
        local max_cpu_used=6
        ;;
      x86)
        # x86 has a good amount of overlap with x86-64. Only a few values are
        # tested to improve the runtime of the script.
        local min_cpu_used=2
        local max_cpu_used=3
        ;;
      *)
        local min_cpu_used=0
        local max_cpu_used=6
        ;;
    esac
    local test_params=av1_encode_good_params
  elif [ "${preset}" = "rt" ]; then
    local min_cpu_used=5
    local max_cpu_used=11
    local test_params=av1_encode_rt_params
  else
    elog "Invalid preset"
    return 1
  fi

  for cpu in $(seq $min_cpu_used $max_cpu_used); do
    if [ "${preset}" = "good" ]; then
      if [ "${arch}" = "x86_64" -o "${arch}" = "arm64" ]; then
        if [ "${cpu}" -lt 2 ]; then
          local test_clips="${LOWBD_CIF_CLIP} ${HIGHBD_CLIP}"
        elif [ "${cpu}" -lt 5 ]; then
          local test_clips="${LOWBD_480p_CLIP} ${HIGHBD_CLIP}"
        else
          local test_clips="${LOWBD_720p_CLIP} ${HIGHBD_CLIP}"
        fi
      elif [ "${arch}" = "x86" ]; then
        local test_clips="${LOWBD_CIF_CLIP} ${HIGHBD_CLIP}"
      else
        elog "Unknown architecture: ${arch}"
        return 1
      fi
    elif [ "${preset}" = "rt" ]; then
      if [ "${cpu}" -lt 8 ]; then
        local test_clips="${LOWBD_CIF_CLIP} ${SC_CLIP}"
      else
        local test_clips="${LOWBD_480p_CLIP} ${SC_CLIP}"
      fi
    else
      elog "Invalid preset"
      return 1
    fi

    for clip in ${test_clips}; do
      local test_bitrates=$(get_bitrates ${clip} ${preset})
      for bitrate in ${test_bitrates}; do
        eval "${encoder}" $($clip) $($test_params) \
        "--limit=${AV1_ENCODE_C_VS_SIMD_TEST_FRAME_LIMIT}" \
        "--cpu-used=${cpu}" "--target-bitrate=${bitrate}" "-o" \
        ${AOM_TEST_OUTPUT_DIR}/Out-${target}-"${clip}"-${preset}-${bitrate}kbps-cpu${cpu}${OUT_FILE_SUFFIX} \
        ${devnull}

        if [ "${target}" != "generic" ]; then
          if ! compare_enc_output ${target} $cpu ${clip} $bitrate ${preset}; then
            # Found a mismatch
            return 1
          fi
        fi
      done
    done
  done
}

av1_test_generic() {
  local arch=$1
  local target="generic"
  if [ $arch = "x86_64" ]; then
    local cmake_command="cmake $LIBAOM_SOURCE_DIR -DAOM_TARGET_CPU=${target}"
  elif [ $arch = "x86" ]; then
    # As AV1 encode output differs for x86 32-bit and 64-bit platforms
    # (BUG=aomedia:3479), the x86 32-bit C-only build is generated separately.
    # The cmake command line option -DENABLE_MMX=0 flag disables all SIMD
    # optimizations, and generates a C-only binary.
    local cmake_command="cmake $LIBAOM_SOURCE_DIR -DENABLE_MMX=0 \
      -DCMAKE_TOOLCHAIN_FILE=${LIBAOM_SOURCE_DIR}/build/cmake/toolchains/i686-linux-gcc.cmake"
  fi

  echo "Build for: Generic ${arch}"
  if ! av1_enc_build "${target}" "${cmake_command}"; then
    return 1
  fi

  for preset in $PRESETS; do
    local encoder="$(av1_enc_tool_path "${target}" "${preset}")"
    av1_enc_test $encoder "${arch}" "${target}" "${preset}"
  done
}

# This function encodes AV1 bitstream by enabling SSE2, SSE3, SSSE3, SSE4_1, SSE4_2, AVX, AVX2 as
# there are no functions with MMX, SSE and AVX512 specialization.
# The value of environment variable 'AOM_SIMD_CAPS_MASK' controls enabling of different instruction
# set extension optimizations. The value of the flag 'AOM_SIMD_CAPS_MASK' and the corresponding
# instruction set extension optimization enabled are as follows:
# SSE4_2 AVX2 AVX SSE4_1 SSSE3 SSE3 SSE2 SSE MMX
#   1     1    1    1      1    1    1    1   1  -> 0x1FF -> Enable AVX2 and lower variants
#   1     0    1    1      1    1    1    1   1  -> 0x17F -> Enable AVX and lower variants
#   1     0    0    1      1    1    1    1   1  -> 0x13F -> Enable SSE4_2 and lower variants
#   0     0    0    1      1    1    1    1   1  -> 0x03F -> Enable SSE4_1 and lower variants
#   0     0    0    0      1    1    1    1   1  -> 0x01F -> Enable SSSE3 and lower variants
#   0     0    0    0      0    1    1    1   1  -> 0x00F -> Enable SSE3 and lower variants
#   0     0    0    0      0    0    1    1   1  -> 0x007 -> Enable SSE2 and lower variants
#   0     0    0    0      0    0    0    1   1  -> 0x003 -> Enable SSE and lower variants
#   0     0    0    0      0    0    0    0   1  -> 0x001 -> Enable MMX
## NOTE: In x86_64 platform, it is not possible to enable sse/mmx/c using "AOM_SIMD_CAPS_MASK" as
#  all x86_64 platforms implement sse2.
av1_test_x86() {
  local arch=$1

  if ! uname -m | grep -q "x86"; then
    elog "Machine architecture is not x86 or x86_64"
    return 0
  fi

  if [ $arch = "x86" ]; then
    local target="x86-linux"
    local cmake_command="cmake \
    $LIBAOM_SOURCE_DIR \
    -DCMAKE_TOOLCHAIN_FILE=${LIBAOM_SOURCE_DIR}/build/cmake/toolchains/i686-linux-gcc.cmake"
  elif [ $arch = "x86_64" ]; then
    local target="x86_64-linux"
    local cmake_command="cmake $LIBAOM_SOURCE_DIR"
  fi

  # Available x86 isa variants: "avx2 avx sse4_2 sse4_1 ssse3 sse3 sse2"
  local x86_isa_variants="avx2 sse4_2 sse2"

  echo "Build for x86: ${target}"
  if ! av1_enc_build "${target}" "${cmake_command}"; then
    return 1
  fi

  for preset in $PRESETS; do
    local encoder="$(av1_enc_tool_path "${target}" "${preset}")"
    for isa in $x86_isa_variants; do
      # Note that if has_x86_isa_extn returns 1, it is false, and vice versa.
      if ! has_x86_isa_extn $isa; then
        echo "${isa} is not supported in this machine"
        continue
      fi
      export AOM_SIMD_CAPS_MASK=$($isa)
      if ! av1_enc_test $encoder "${arch}" "${target}" "${preset}"; then
        # Found a mismatch
        return 1
      fi
      unset AOM_SIMD_CAPS_MASK
    done
  done
}

av1_test_arm() {
  local arch="arm64"
  local target="arm64-linux-gcc"
  local cmake_command="cmake $LIBAOM_SOURCE_DIR \
        -DCMAKE_TOOLCHAIN_FILE=$LIBAOM_SOURCE_DIR/build/cmake/toolchains/${target}.cmake \
        -DCMAKE_C_FLAGS=-Wno-maybe-uninitialized"
  echo "Build for arm64: ${target}"
  if ! av1_enc_build "${target}" "${cmake_command}"; then
    return 1
  fi

  for preset in $PRESETS; do
    local encoder="$(av1_enc_tool_path "${target}" "${preset}")"
    if ! av1_enc_test "qemu-aarch64 -L /usr/aarch64-linux-gnu ${encoder}" "${arch}" "${target}" "${preset}"; then
      # Found a mismatch
      return 1
    fi
  done
}

av1_c_vs_simd_enc_test () {
  # Test x86 (32 bit)
  # x86 requires the i686-linux-gnu toolchain:
  # $ sudo apt-get install g++-i686-linux-gnu
  echo "av1 test for x86 (32 bit): Started."
  # Encode 'C' only
  av1_test_generic "x86"
  # Encode with SIMD optimizations enabled
  if ! av1_test_x86 "x86"; then
    echo "av1 test for x86 (32 bit): Done, test failed."
    return 1
  else
    echo "av1 test for x86 (32 bit): Done, all tests passed."
  fi

  # Test x86_64 (64 bit)
  if [ "$(eval uname -m)" = "x86_64" ]; then
    echo "av1 test for x86_64 (64 bit): Started."
    # Encode 'C' only
    av1_test_generic "x86_64"
    # Encode with SIMD optimizations enabled
    if ! av1_test_x86 "x86_64"; then
      echo "av1 test for x86_64 (64 bit): Done, test failed."
      return 1
    else
      echo "av1 test for x86_64 (64 bit): Done, all tests passed."
    fi
  fi

  # Test ARM
  echo "av1_test_arm: Started."
  if ! av1_test_arm; then
    echo "av1 test for arm: Done, test failed."
    return 1
  else
    echo "av1 test for arm: Done, all tests passed."
  fi
}

run_tests av1_c_vs_simd_enc_verify_environment av1_c_vs_simd_enc_test
