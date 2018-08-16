#!/bin/sh
set -e

WEB_CONTENT_RESOURCES_PATH="${BUILT_PRODUCTS_DIR}/WebKit.framework/PrivateHeaders/CustomWebContentResources"
mkdir -p "${WEB_CONTENT_RESOURCES_PATH}"

echo "Copying WebContent entitlements to ${WEB_CONTENT_RESOURCES_PATH}"
ENTITLEMENTS_FILE="${TEMP_FILE_DIR}/${FULL_PRODUCT_NAME}.xcent"
ditto "${ENTITLEMENTS_FILE}" "${WEB_CONTENT_RESOURCES_PATH}/WebContent.entitlements"

echo "Copying WebContentProcess.xib to ${WEB_CONTENT_RESOURCES_PATH}"
WEBCONTENT_XIB="${SRCROOT}/Resources/WebContentProcess.xib"
ditto "${WEBCONTENT_XIB}" "${WEB_CONTENT_RESOURCES_PATH}/WebContentProcess.xib"

echo "Copying Info.plist to ${WEB_CONTENT_RESOURCES_PATH}"
PROCESSED_INFOPLIST="${BUILT_PRODUCTS_DIR}/${INFOPLIST_PATH}"
UNPROCESSED_INFOPLIST="${INFOPLIST_FILE}"
COPIED_INFOPLIST="${WEB_CONTENT_RESOURCES_PATH}/Info.plist"
ditto "${UNPROCESSED_INFOPLIST}" "${COPIED_INFOPLIST}"

echo "Setting fixed entry values for ${COPIED_INFOPLIST}"
if [[ ${WK_PLATFORM_NAME} == "macosx" ]]; then
    FIXED_ENTRIES=( ":XPCService:RunLoopType" )
else
    FIXED_ENTRIES=()
fi

for ((i = 0; i < ${#FIXED_ENTRIES[@]}; ++i)); do
    ENTRY_VALUE=$(/usr/libexec/PlistBuddy -c "Print ${FIXED_ENTRIES[$i]}" "${PROCESSED_INFOPLIST}")
    /usr/libexec/PlistBuddy -c "Set ${FIXED_ENTRIES[$i]} ${ENTRY_VALUE}" "${COPIED_INFOPLIST}"
done
