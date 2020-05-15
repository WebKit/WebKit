#!/bin/bash

#  Copyright The ANGLE Project Authors. All rights reserved.
#  Use of this source code is governed by a BSD-style license that can be
#  found in the LICENSE file.
#
# Generates a roll CL within the ANGLE repository of AOSP.

git merge -X theirs aosp/upstream-mirror

deps=(
    "third_party/spirv-tools/src"
    "third_party/glslang/src"
    "third_party/spirv-headers/src"
    "third_party/vulkan-headers/src"
    "third_party/jsoncpp"
    "third_party/jsoncpp/source"
)

# Delete dep directories so that gclient can check them out
for dep in ${deps[@]}; do
    rm -rf --preserve-root $dep
done

# Sync all of ANGLE's deps so that 'gn gen' works
python scripts/bootstrap.py
gclient sync -D

# generate gn build files and convert them to blueprints
gn_args=(
    "target_os = \"android\""
    "is_component_build = false"
    "is_debug = false"

    # Build for 64-bit CPUs
    "target_cpu = \"arm64\""

    # Don't make a dependency on .git/HEAD. Some Android builds are done without .git folders
    # present.
    "angle_enable_commit_id = false"

    # Target ndk API 26 to make sure ANGLE can use the Vulkan backend on Android
    "android32_ndk_api_level = 26"
    "android64_ndk_api_level = 26"

    # Disable all backends except Vulkan
    "angle_enable_vulkan = true"
    "angle_enable_gl = true" # TODO(geofflang): Disable GL once Andrid no longer requires it. anglebug.com/4444
    "angle_enable_d3d9 = false"
    "angle_enable_d3d11 = false"
    "angle_enable_null = false"
    "angle_enable_metal = false"

    # SwiftShader is loaded as the system Vulkan driver on Android, not compiled by ANGLE
    "angle_enable_swiftshader = false"

    # Disable all shader translator targets except desktop GL (for Vulkan)
    "angle_enable_essl = true" # TODO(geofflang): Disable ESSL once Andrid no longer requires it. anglebug.com/4444
    "angle_enable_glsl = true" # TODO(geofflang): Disable ESSL once Andrid no longer requires it. anglebug.com/4444
    "angle_enable_hlsl = false"
)
gn gen out/Android --args="${gn_args[*]}"
gn desc out/Android --format=json "*" > out/Android/desc.json
python scripts/generate_android_bp.py out/Android/desc.json > Android.bp
rm -r out
git add Android.bp

# Delete the .git files in each dep so that it can be added to this repo. Some deps like jsoncpp
# have multiple layers of deps so delete everything before adding them.
for dep in ${deps[@]}; do
    rm -rf --preserve-root $dep/.git
done

for dep in ${deps[@]}; do
    git add -f $dep
done

git commit --amend --no-edit
