#!/bin/sh -e

# Create the product header.
sed -E -e 's/<WebCore\//<WebKitLegacy\//' -e "s/(^ *)WEBCORE_EXPORT /\1/" "${INPUT_FILE_PATH}" > "${SCRIPT_OUTPUT_FILE_0}"

# *_SEARCH_PATHS are already shell-escaped, convert to an array so we can pass a flag for each path.
eval HEADER_SEARCH_PATHS=(${HEADER_SEARCH_PATHS} ${SYSTEM_HEADER_SEARCH_PATHS})
# Work around rdar://91303280 when building STP.
eval FRAMEWORK_SEARCH_PATHS=(${FRAMEWORK_SEARCH_PATHS//${WK_OVERRIDE_FRAMEWORKS_DIR}/${WK_QUOTED_OVERRIDE_FRAMEWORKS_DIR}} ${SYSTEM_FRAMEWORK_SEARCH_PATHS})

# Create an export list, which will be used by "Generate Export Files" to create an export symbols
# list that includes symbols from this header.
for WK_CURRENT_ARCH in ${ARCHS}; do
    tapi reexport -target ${WK_CURRENT_ARCH}-${LLVM_TARGET_TRIPLE_VENDOR}-${LLVM_TARGET_TRIPLE_OS_VERSION}${LLVM_TARGET_TRIPLE_SUFFIX} -isysroot ${SDK_DIR} -I${BUILT_PRODUCTS_DIR} "${HEADER_SEARCH_PATHS[@]/#/-I}" -F${BUILT_PRODUCTS_DIR} "${FRAMEWORK_SEARCH_PATHS[@]/#/-F}" -DWEBCORE_EXPORT= "${SDK_DIR}/usr/include/TargetConditionals.h" "${INPUT_FILE_PATH}" -o /dev/stdout
done >> "${TARGET_TEMP_DIR}/ReexportedFromWebCore.exp"
