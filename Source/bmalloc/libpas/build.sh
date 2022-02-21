#!/bin/sh
# Copyright (c) 2018-2021 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer. 
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution. 
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

. common.sh

build_variant_xcodebuild() {
    variant=$1
    target=$2
    extra_xcodebuild_options="EXTRA_DEFINES=$3"
    
    build_dir=build-$sdk-$variant
    mkdir -p $build_dir
    
    xcodebuild_options="-project libpas.xcodeproj -configuration $config -sdk $sdk OBJROOT=$build_dir SYMROOT=$build_dir $extra_xcodebuild_options"

    if [ "x$archs" != "xblank" ]
    then
        xcodebuild_options="$xcodebuild_options ARCHS=$archs"
    fi
    
    xcodebuild $xcodebuild_options -target $target
            
    case $target in
        mbmalloc|all)
            link_mbmalloc_personality() {
                personality=$1
                
                mkdir -p $build_dir/$configdir/mbmalloc_$personality
                ln -fs $PWD/$build_dir/$configdir/libpas.dylib $build_dir/$configdir/mbmalloc_$personality
                ln -fs $PWD/$build_dir/$configdir/libpas_verifier.dylib $build_dir/$configdir/mbmalloc_$personality
                ln -fs $PWD/$build_dir/$configdir/libmbmalloc_$personality.dylib $build_dir/$configdir/mbmalloc_$personality/libmbmalloc.dylib
            }
            
            link_mbmalloc_personality iso_common_primitive
            link_mbmalloc_personality iso_common_primitive_verified
            ;;
        *)
            ;;
    esac
}

build_variant_cmake() {
    variant=$1
    target=$2
    extra_options=$3

    case $config in
        Release)
            ;;
        Debug)
            ;;
        *)
            echo "Bad value of config"
            exit 1
    esac

    build_dir=build-$sdk-$variant/$configdir
    mkdir -p $build_dir

    extra_cmake_options=""
    for option in $extra_options; do
        extra_cmake_options="${extra_cmake_options} -D${option}"
    done
    extra_cmake_options=\"${extra_cmake_options#* }\"

    echo $extra_cmake_options

    cmake -DCMAKE_BUILD_TYPE=$config -DCMAKE_C_FLAGS=$extra_cmake_options -DCMAKE_CXX_FLAGS=$extra_cmake_options -B $build_dir
    cmake --build $build_dir --parallel
}

build_variant() {
    case $sdk in
        cmake)
            build_variant_cmake $@
            ;;
        *)
            build_variant_xcodebuild $@
            ;;
    esac
}

build_variants() {
    target=$1
    
    case $variants in
        all|testing)
            build_variant testing $target "ENABLE_PAS_TESTING"
            ;;
    esac
    case $variants in
        all|default)
            build_variant default $target ""
            ;;
    esac
}

build_variants $target

