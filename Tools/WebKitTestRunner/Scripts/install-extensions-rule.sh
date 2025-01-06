set -e

DESTINATION_PATH=${BUILT_PRODUCTS_DIR}/WebKitTestRunnerApp.app/Extensions/${INPUT_FILE_NAME}

mkdir -p "${DESTINATION_PATH}"
ditto "${BUILT_PRODUCTS_DIR}/${INPUT_FILE_NAME}" "${DESTINATION_PATH}"

/usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier ${PRODUCT_BUNDLE_IDENTIFIER}.${INPUT_FILE_BASE}" "${DESTINATION_PATH}/Info.plist"

if [[ ${INPUT_FILE_BASE} == "WebContentExtension" || ${INPUT_FILE_BASE} == "NetworkingExtension" ]]; then
    /usr/libexec/PlistBuddy -c "Add :NSAppTransportSecurity:NSAllowsArbitraryLoads bool true" "${DESTINATION_PATH}/Info.plist"
fi

if [[ ${INPUT_FILE_BASE} == "WebContentExtension" ]]; then
    EXTENSION_POINT_ID="com.apple.web-browser-engine.content"
elif [[ ${INPUT_FILE_BASE} == "WebContentCaptivePortalExtension" ]]; then
    EXTENSION_POINT_ID="com.apple.web-browser-engine.content"
fi

if [[ -n "${EXTENSION_POINT_ID}" ]]; then
    /usr/libexec/PlistBuddy -c "Delete :EXAppExtensionAttributes:EXExtensionPointIdentifier dict" "${DESTINATION_PATH}/Info.plist"
    /usr/libexec/PlistBuddy -c "Add :EXAppExtensionAttributes:EXExtensionPointIdentifier string ${EXTENSION_POINT_ID}" "${DESTINATION_PATH}/Info.plist"
fi

if [ -n "${CODE_SIGN_IDENTITY}" ]; then
    xcrun codesign --force --preserve-metadata=entitlements --sign "${CODE_SIGN_IDENTITY}" ${OTHER_CODE_SIGN_FLAGS} "${DESTINATION_PATH}"
fi
