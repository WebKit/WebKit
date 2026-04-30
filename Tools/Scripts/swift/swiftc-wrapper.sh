#!/bin/bash
# cmake accumulates CFLAGS from pkg-config, and then passes them to swiftc.
# This script filters out the arguments that swiftc cannot accommodate.

set -e
set -o pipefail

# Swift's C++ interop changes which imported members are @unsafe between
# toolchain versions, so an `unsafe` that is required on one toolchain emits
# "no unsafe operations occur within 'unsafe' expression" on another. The
# diagnostic has no group, so it can't be suppressed with -Wwarning; filter it
# (and its multi-line source snippet) from stderr instead.
filter_benign_warnings() {
    awk '
        /: warning: no unsafe operations occur within .unsafe. expression/ { skip = 1; next }
        skip && /^[[:space:]]*[0-9]*[[:space:]]*\|/ { next }
        skip && /^[[:space:]]*$/ { skip = 0; next }
        { skip = 0; print }
    '
}

REAL_SWIFTC=swiftc
args=()

# CMake's Swift link rule injects <LANGUAGE_COMPILE_FLAGS> into the link
# command, which includes -g, which causes swiftc to run dsymutil. 
# dsymutil is super expensive, and we don't need it because we have DWARF
# debug info in our object files.
linking=
for arg in "$@"; do
    case "$arg" in
        "-emit-library"|"-emit-executable") linking=1 ;;
    esac
done

for arg in "$@"; do
    case "$arg" in
        "-mfpmath=sse") ;;
        "-msse") ;;
        "-msse2") ;;
        "-pthread") ;;
        "-fsanitize="*)
            args+=("-sanitize=${arg#-fsanitize=}")
            ;;
        "-g")
            if [[ -z "$linking" ]]; then
                args+=("$arg")
            fi
            ;;
        "-include") skip_next=1 ;;
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
        "-D"*)
            args+=("$arg" "-Xcc" "$arg")
            ;;
        *)
            if [[ -n "$skip_next" ]]; then
                skip_next=
            elif [[ -n "$skip_next_as_xlinker" ]]; then
                args+=("-Xlinker" "$arg")
                skip_next_as_xlinker=
            else
                args+=("$arg")
            fi
            ;;
    esac
done

{ "$REAL_SWIFTC" "${args[@]}" 2>&1 1>&3 | filter_benign_warnings >&2; } 3>&1
