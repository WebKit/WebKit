#!/bin/bash
# cmake accumulates CFLAGS from pkg-config, and then passes them to swiftc.
# This script filters out the arguments that swiftc cannot accommodate.

set -e

REAL_SWIFTC=swiftc
args=()

for arg in "$@"; do
    case "$arg" in
        "-mfpmath=sse") ;;
        "-msse") ;;
        "-msse2") ;;
        "-pthread") ;;
        "-include") skip_next=1 ;;
        # Translate clang's -iframework to swiftc's -Fsystem.
        "-iframework"*)
            args+=("-Fsystem" "${arg#-iframework}")
            ;;
        # Translate clang's -isystem to swiftc's -Xcc -isystem -Xcc.
        "-isystem"*)
            args+=("-Xcc" "-isystem" "-Xcc" "${arg#-isystem}")
            ;;
        # CMake leaks clang linker flags into swiftc; translate them.
        "-compatibility_version"|"-current_version")
            args+=("-Xlinker" "$arg")
            skip_next_as_xlinker=1
            ;;
        "-weak_framework")
            args+=("-Xlinker" "-weak_framework")
            skip_next_as_xlinker=1
            ;;
        "-Wl,"*)
            # Split -Wl,arg1,arg2 into -Xlinker arg1 -Xlinker arg2
            IFS=',' read -ra _wl_args <<< "${arg#-Wl,}"
            for _wl in "${_wl_args[@]}"; do
                args+=("-Xlinker" "$_wl")
            done
            ;;
        "--original-swift-compiler="*)
            REAL_SWIFTC="${arg#--original-swift-compiler=}"
            ;;
        "-target")
            capture_target=1
            args+=("$arg")
            ;;
        "-D"*)
            args+=("$arg" "-Xcc" "$arg")
            ;;
        *)
            if [[ -n "$skip_next" ]]; then
                skip_next=
            elif [[ -n "$skip_next_as_xlinker" ]]; then
                args+=("-Xlinker" "$arg")
                skip_next_as_xlinker=
            elif [[ -n "$capture_target" ]]; then
                args+=("$arg" "-clang-target" "$arg")
                capture_target=
            else
                args+=("$arg")
            fi
            ;;
    esac
done

exec "$REAL_SWIFTC" "${args[@]}"
