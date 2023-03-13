#!/bin/sh

if [ -z "${PROFILE_DATA_FLAGS}" ]; then
    exit 0;
fi

profiles="${SCRIPT_INPUT_FILE_0}"
# The fallback profile (empty profdata) is used in open source builds, and in
# engineering builds when the selected profile is in an unrecognized format
# (i.e. git-lfs is not installed).
fallback_decompressed_profdata="${SCRIPT_INPUT_FILE_1}"
derived_decompressed_profdata="${SCRIPT_OUTPUT_FILE_0}"

# Search for a PGO profile corresponding to the current target triple or a
# previous OS version triple. In lieu of an exact match, fall back to the most
# recent macOS profile.
#
# FIXME: Don't assume that profiles only exist for arm64e. This script runs as
# a target build phase, so there is no CURRENT_ARCH, but it could process each
# value of ARCHS.
# FIXME: Support LLVM_TARGET_TRIPLE_SUFFIX to have separate profiles for
# simulators and catalyst. We don't need this, but it is easy to add.

function lte_os_version {
    while read candidate_triple; do
        printf '%s\n' ${candidate_triple#${profiles}/arm64e-apple-} ${LLVM_TARGET_TRIPLE_OS_VERSION} | sort -CV &&
            echo ${candidate_triple}
        done
}

pgo_target_triples=(
    # SWIFT_PLATFORM_TARGET_PREFIX is LLVM_TARGET_TRIPLE_OS_VERSION minus the
    # version number. If an exact match for this OS version is available, it
    # will be chosen.
    $(printf '%s\n' "${profiles}/arm64e-apple-${SWIFT_PLATFORM_TARGET_PREFIX}"* | sort -rV | lte_os_version)
    # The latest macOS triple.
    $(printf '%s\n' "${profiles}/arm64e-apple-macos"* | sort -rV | head -1)
)

for pgo_triple in "${pgo_target_triples[@]}"; do
    [ -d "${pgo_triple}" ] && break
done
if [ $? -eq 0 ]; then
    # Can't use TARGET_NAME to select which profdata to use, because this
    # script may run as part of a "Derived Sources" target.
    input_profdata="${pgo_triple}/${PROJECT_NAME}.profdata.compressed"
elif [ "${USE_INTERNAL_SDK}" != YES ]; then
    input_profdata="${fallback_decompressed_profdata}"
else
    echo "error: could not find a suitable profile in any of the following directories:" "${pgo_target_triples[@]}"
    exit 1
fi
input_profdata_type="$(file -b "${input_profdata}")"

if [[ "${input_profdata_type}" = "lzfse compressed"* ]]; then
    set -x; compression_tool -decode -i "${input_profdata}" -o "${derived_decompressed_profdata}" -a lzfse
elif [[ "${input_profdata_type}" = "LLVM indexed profile data"* ]]; then
    set -x; cp "${input_profdata}" "${derived_decompressed_profdata}"
elif [ "${CONFIGURATION}" != Production ] && [ "${input_profdata}" != "${fallback_decompressed_profdata}" ]; then
    echo "warning: unrecognized profiling data at ${input_profdata}, falling back to stub data"
    set -x; cp "${fallback_decompressed_profdata}" "${derived_decompressed_profdata}"
else
    echo "error: unrecognized profiling data at ${input_profdata}"
    exit 1
fi

