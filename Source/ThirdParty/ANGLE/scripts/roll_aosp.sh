#!/bin/bash

#  Copyright The ANGLE Project Authors. All rights reserved.
#  Use of this source code is governed by a BSD-style license that can be
#  found in the LICENSE file.
#
# Generates a roll CL within the ANGLE repository of AOSP.
#
# WARNING: Running this script without args may mess up the checkout.
#   See --genAndroidBp for testing just the code generation.

# exit when any command fails
set -eE -o functrace

failure() {
  local lineno=$1
  local msg=$2
  echo "Failed at $lineno: $msg"
}
trap 'failure ${LINENO} "$BASH_COMMAND"' ERR

# Change the working directory to the ANGLE root directory
cd "${0%/*}/.."

GN_OUTPUT_DIRECTORY=out/Android

function generate_Android_bp_file() {
    abis=(
        "arm"
        "arm64"
        "x86"
        "x64"
    )

    for abi in "${abis[@]}"; do
        # generate gn build files and convert them to blueprints
        gn_args=(
            "target_os = \"android\""
            "is_component_build = false"
            "is_debug = false"
            "dcheck_always_on = false"
            "symbol_level = 0"
            "angle_standalone = false"
            "angle_build_all = false"
            "angle_expose_non_conformant_extensions_and_versions = true"

            # Build for 64-bit CPUs
            "target_cpu = \"$abi\""

            # Target ndk API 26 to make sure ANGLE can use the Vulkan backend on Android
            "android32_ndk_api_level = 26"
            "android64_ndk_api_level = 26"

            # Disable all backends except Vulkan
            "angle_enable_vulkan = true"
            "angle_enable_gl = false"
            "angle_enable_d3d9 = false"
            "angle_enable_d3d11 = false"
            "angle_enable_null = false"
            "angle_enable_metal = false"
            "angle_enable_wgpu = false"

            # SwiftShader is loaded as the system Vulkan driver on Android, not compiled by ANGLE
            "angle_enable_swiftshader = false"

            # Disable all shader translator targets except desktop GL (for Vulkan)
            "angle_enable_essl = false"
            "angle_enable_glsl = false"
            "angle_enable_hlsl = false"

            "angle_enable_commit_id = false"

            # Disable histogram/protobuf support
            "angle_has_histograms = false"

            # Use system lib(std)c++, since the Chromium library breaks std::string
            "use_custom_libcxx = false"

            # rapidJSON is used for ANGLE's frame capture (among other things), which is unnecessary for AOSP builds.
            "angle_has_rapidjson = false"
        )

        if [[ "$1" == "--enableApiTrace" ]]; then
            gn_args=(
                "${gn_args[@]}"
                "angle_enable_trace = true"
                "angle_enable_trace_android_logcat = true"
            )
        fi

        gn gen ${GN_OUTPUT_DIRECTORY} --args="${gn_args[*]}"
        gn desc ${GN_OUTPUT_DIRECTORY} --format=json "*" > ${GN_OUTPUT_DIRECTORY}/desc.$abi.json
    done

    python3 scripts/generate_android_bp.py \
        --gn_json_arm=${GN_OUTPUT_DIRECTORY}/desc.arm.json \
        --gn_json_arm64=${GN_OUTPUT_DIRECTORY}/desc.arm64.json \
        --gn_json_x86=${GN_OUTPUT_DIRECTORY}/desc.x86.json \
        --gn_json_x64=${GN_OUTPUT_DIRECTORY}/desc.x64.json \
        --output=Android.bp
}


if [[ "$1" == "--genAndroidBp" ]];then
    generate_Android_bp_file "$2"
    exit 0
fi

# Check out depot_tools locally and add it to the path
DEPOT_TOOLS_DIR=_depot_tools
rm -rf ${DEPOT_TOOLS_DIR}
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git ${DEPOT_TOOLS_DIR}
export PATH=`pwd`/${DEPOT_TOOLS_DIR}:$PATH

third_party_deps=(
    "build"
    "third_party/abseil-cpp"
    "third_party/vulkan-deps/glslang/src"
    "third_party/vulkan-deps/spirv-headers/src"
    "third_party/vulkan-deps/spirv-tools/src"
    "third_party/vulkan-deps/vulkan-headers/src"
    "third_party/vulkan_memory_allocator"
)

root_add_deps=(
  "build"
  "third_party"
)

# Only add the parts of NDK and vulkan-deps that are required by ANGLE. The entire dep is too large.
delete_only_deps=(
    "third_party/vulkan-deps"
    "third_party/zlib"  # Replaced by Android's zlib; delete for gclient to work https://crbug.com/skia/14155#c3
)

# Delete dep directories so that gclient can check them out
for dep in "${third_party_deps[@]}" "${delete_only_deps[@]}"; do
    rm -rf "$dep"
done

# Remove cruft from any previous bad rolls (https://anglebug.com/8352)
extra_third_party_removal_patterns=(
   "*/_gclient_*"
)

for removal_dir in "${extra_third_party_removal_patterns[@]}"; do
    find third_party -wholename "$removal_dir" -delete
done

# Sync all of ANGLE's deps so that 'gn gen' works
python scripts/bootstrap.py
gclient sync --reset --force --delete_unversioned_trees

# Delete outdir to ensure a clean gn run.
rm -rf ${GN_OUTPUT_DIRECTORY}

generate_Android_bp_file

# Delete outdir to cleanup after gn.
rm -rf ${GN_OUTPUT_DIRECTORY}

# Delete all unsupported 3rd party dependencies. Do this after generate_Android_bp_file, so
# it has access to all of the necessary BUILD.gn files.
unsupported_third_party_deps=(
   "third_party/jdk"
   "third_party/llvm-build"
   "third_party/android_build_tools"
   "third_party/android_sdk"
   "third_party/android_toolchain"
   "third_party/zlib"  # Replaced by Android's zlib
)
for unsupported_third_party_dep in "${unsupported_third_party_deps[@]}"; do
   rm -rf "$unsupported_third_party_dep"
done

git add Android.bp

# Delete the .git files in each dep so that it can be added to this repo. Some deps like jsoncpp
# have multiple layers of deps so delete everything before adding them.
for dep in "${third_party_deps[@]}"; do
   rm -rf "$dep"/.git
done

# Delete all the .gitmodules files, since they are not allowed in AOSP external projects.
find . -name \.gitmodules -exec rm {} \;

extra_removal_files=(
   # build/linux is hundreds of megs that aren't needed.
   "build/linux"
   # Debuggable APKs cannot be merged into AOSP as a prebuilt
   "build/android/CheckInstallApk-debug.apk"
   # Remove Android.mk files to prevent automated CLs:
   #   "[LSC] Add LOCAL_LICENSE_KINDS to external/angle"
   "Android.mk"
   "third_party/vulkan-deps/glslang/src/Android.mk"
   "third_party/vulkan-deps/glslang/src/ndk_test/Android.mk"
   "third_party/vulkan-deps/spirv-tools/src/Android.mk"
   "third_party/vulkan-deps/spirv-tools/src/android_test/Android.mk"
   "third_party/siso" # Not needed
)

for removal_file in "${extra_removal_files[@]}"; do
   rm -rf "$removal_file"
done

# Add all changes under $root_add_deps so we delete everything not explicitly allowed.
for root_add_dep in "${root_add_deps[@]}"; do
    git add -f $root_add_dep
done

# Done with depot_tools
rm -rf $DEPOT_TOOLS_DIR
