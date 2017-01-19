#!/bin/bash

#  Copyright 2015 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

# Generates static FAT libraries for ios in out_ios_libs.

# Exit on errors.
set -e

# Environment
export PATH=/usr/libexec:$PATH

SCRIPT_DIR=$(cd $(dirname $0) && pwd)
WEBRTC_BASE_DIR=${SCRIPT_DIR}/../../..

function clean_artifacts {
  local output_dir=$1
  if [[ -d ${output_dir} ]]; then
    echo "Deleting ${output_dir}"
    rm -r ${output_dir}
  fi
}

function build_webrtc {
  local target_arch=$1
  local flavor=$2
  local build_type=$3
  local ios_deployment_target=$4
  local libvpx_build_vp9=$5
  local custom_gn_options=$6

  OUTPUT_DIR=${SDK_OUTPUT_DIR}/${target_arch}_libs
  GN_ARGS="target_os=\"ios\" ios_enable_code_signing=false \
use_xcode_clang=true is_component_build=false rtc_ios_enable_bitcode=true"

  # Add flavor option.
  if [[ ${flavor} = "debug" ]]; then
    GN_ARGS="${GN_ARGS} is_debug=true"
  elif [[ ${flavor} = "release" ]]; then
    GN_ARGS="${GN_ARGS} is_debug=false"
  else
    echo "Unexpected flavor type: ${flavor}"
    exit 1
  fi

  # Add the specified architecture.
  OUTPUT_LIB=${OUTPUT_DIR}/${SDK_LIB_NAME}
  GN_ARGS="${GN_ARGS} target_cpu=\"${target_arch}\""

  # Add deployment target.
  GN_ARGS="${GN_ARGS} ios_deployment_target=\"${ios_deployment_target}\""

  # Add vp9 option.
  GN_ARGS="${GN_ARGS} rtc_libvpx_build_vp9=${libvpx_build_vp9}"

  # Add custom options.
  if [[ -n "${custom_gn_options}" ]]; then
    GN_ARGS="${GN_ARGS} ${custom_gn_options}"
  fi

  # Generate static or dynamic.
  if [[ ${build_type} = "static_only" ]]; then
    GN_TARGET_NAME="rtc_sdk_objc"
  elif [[ ${build_type} == "framework" ]]; then
    GN_TARGET_NAME="rtc_sdk_framework_objc"
    GN_ARGS="${GN_ARGS} enable_dsyms=true enable_stripping=true"
  fi

  echo "Building WebRTC with args: ${GN_ARGS}"
  gn gen ${OUTPUT_DIR} --args="${GN_ARGS}"
  echo "Building target: ${GN_TARGET_NAME}"
  ninja -C ${OUTPUT_DIR} ${GN_TARGET_NAME}

  # Strip debug symbols to reduce size.
  if [[ ${build_type} = "static_only" ]]; then
    strip -S ${OUTPUT_DIR}/obj/webrtc/sdk/lib${GN_TARGET_NAME}.a -o \
        ${OUTPUT_DIR}/lib${GN_TARGET_NAME}.a
  fi
}

function usage {
  echo "WebRTC iOS FAT libraries build script."
  echo "Each architecture is compiled separately before being merged together."
  echo "By default, the fat libraries will be created in out_ios_libs/."
  echo "The headers will be copied to out_ios_libs/include."
  echo "Usage: $0 [-h] [-b build_type] [-c] [-o output_dir]"
  echo "  -h Print this help."
  echo "  -b The build type. Can be framework or static_only."
  echo "     Defaults to framework."
  echo "  -c Removes generated build output."
  echo "  -o Specifies a directory to output build artifacts to."
  echo "     If specified together with -c, deletes the dir."
  echo "  -r Specifies a revision number to embed if building the framework."
  exit 0
}

SDK_OUTPUT_DIR=${WEBRTC_BASE_DIR}/out_ios_libs
SDK_LIB_NAME="librtc_sdk_objc.a"
SDK_FRAMEWORK_NAME="WebRTC.framework"

BUILD_FLAVOR="release"
BUILD_TYPE="framework"
ENABLED_ARCHITECTURES=("arm" "arm64" "x64")
IOS_DEPLOYMENT_TARGET="8.0"
LIBVPX_BUILD_VP9="false"
CUSTOM_GN_OPTS=""
WEBRTC_REVISION="0"

# Parse arguments.
while getopts "hb:co:r:" opt; do
  case "${opt}" in
    h) usage;;
    b) BUILD_TYPE="${OPTARG}";;
    c) PERFORM_CLEAN=1;;
    o) SDK_OUTPUT_DIR="${OPTARG}";;
    r) WEBRTC_REVISION="${OPTARG}";;
    *)
      usage
      exit 1
      ;;
  esac
done

if [[ ${PERFORM_CLEAN} -ne 0 ]]; then
  clean_artifacts ${SDK_OUTPUT_DIR}
  exit 0
fi

# Build all architectures.
for arch in ${ENABLED_ARCHITECTURES[*]}; do
    build_webrtc $arch ${BUILD_FLAVOR} ${BUILD_TYPE} \
                 ${IOS_DEPLOYMENT_TARGET} ${LIBVPX_BUILD_VP9} ${CUSTOM_GN_OPTS}
done

# Ignoring x86 except for static libraries for now because of a GN build issue
# where the generated dynamic framework has the wrong architectures.

# Create FAT archive.
if [[ ${BUILD_TYPE} = "static_only" ]]; then
  build_webrtc "x86" ${BUILD_FLAVOR} ${BUILD_TYPE} \
               ${IOS_DEPLOYMENT_TARGET} ${LIBVPX_BUILD_VP9} ${CUSTOM_GN_OPTS}

  ARM_LIB_PATH=${SDK_OUTPUT_DIR}/arm_libs/${SDK_LIB_NAME}
  ARM64_LIB_PATH=${SDK_OUTPUT_DIR}/arm64_libs/${SDK_LIB_NAME}
  X64_LIB_PATH=${SDK_OUTPUT_DIR}/x64_libs/${SDK_LIB_NAME}
  X86_LIB_PATH=${SDK_OUTPUT_DIR}/x86_libs/${SDK_LIB_NAME}

  # Combine the slices.
  lipo ${ARM_LIB_PATH} ${ARM64_LIB_PATH} ${X64_LIB_PATH} ${X86_LIB_PATH} \
       -create -output ${SDK_OUTPUT_DIR}/${SDK_LIB_NAME}
elif [[ ${BUILD_TYPE} = "framework" ]]; then
  ARM_LIB_PATH=${SDK_OUTPUT_DIR}/arm_libs
  ARM64_LIB_PATH=${SDK_OUTPUT_DIR}/arm64_libs
  X64_LIB_PATH=${SDK_OUTPUT_DIR}/x64_libs
  X86_LIB_PATH=${SDK_OUTPUT_DIR}/x86_libs

  # Combine the slices.
  DYLIB_PATH="WebRTC.framework/WebRTC"
  cp -R ${ARM64_LIB_PATH}/WebRTC.framework ${SDK_OUTPUT_DIR}
  rm ${SDK_OUTPUT_DIR}/${DYLIB_PATH}
  echo "Merging framework slices."
  lipo ${ARM_LIB_PATH}/${DYLIB_PATH} \
       ${ARM64_LIB_PATH}/${DYLIB_PATH} \
       ${X64_LIB_PATH}/${DYLIB_PATH} \
       -create -output ${SDK_OUTPUT_DIR}/${DYLIB_PATH}

  # Remove stray mobileprovision if it exists until chromium roll lands.
  # See https://codereview.chromium.org/2397433002.
  PROVISION_FILE=${SDK_OUTPUT_DIR}/WebRTC.framework/embedded.mobileprovision
  if [[ -e ${PROVISION_FILE} ]]; then
    rm ${PROVISION_FILE}
  fi

  # Merge the dSYM slices.
  DSYM_PATH="WebRTC.dSYM/Contents/Resources/DWARF/WebRTC"
  cp -R ${ARM64_LIB_PATH}/WebRTC.dSYM ${SDK_OUTPUT_DIR}
  rm ${SDK_OUTPUT_DIR}/${DSYM_PATH}
  echo "Merging dSYM slices."
  lipo ${ARM_LIB_PATH}/${DSYM_PATH} \
      ${ARM64_LIB_PATH}/${DSYM_PATH} \
      ${X64_LIB_PATH}/${DSYM_PATH} \
      -create -output ${SDK_OUTPUT_DIR}/${DSYM_PATH}

  # Modify the version number.
  # Format should be <Branch cut MXX>.<Hotfix #>.<Rev #>.
  # e.g. 55.0.14986 means branch cut 55, no hotfixes, and revision number 14986.
  INFOPLIST_PATH=${SDK_OUTPUT_DIR}/WebRTC.framework/Info.plist
  MAJOR_MINOR=$(PlistBuddy -c "Print :CFBundleShortVersionString" \
                ${INFOPLIST_PATH})
  VERSION_NUMBER="${MAJOR_MINOR}.${WEBRTC_REVISION}"
  echo "Substituting revision number: ${VERSION_NUMBER}"
  PlistBuddy -c "Set :CFBundleVersion ${VERSION_NUMBER}" ${INFOPLIST_PATH}
  plutil -convert binary1 ${INFOPLIST_PATH}
else
  echo "BUILD_TYPE ${BUILD_TYPE} not supported."
  exit 1
fi

echo "Done."
