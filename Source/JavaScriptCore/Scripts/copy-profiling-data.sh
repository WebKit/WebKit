#!/bin/sh

if [ -z "${PROFILE_DATA_FLAGS}" ]; then
    exit 0;
fi

input_profdata="${SCRIPT_INPUT_FILE_0}"
input_profdata_type="$(file -b "${input_profdata}")"

fallback_decompressed_profdata="${SCRIPT_INPUT_FILE_1}"
derived_decompressed_profdata="${SCRIPT_OUTPUT_FILE_0}"

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

