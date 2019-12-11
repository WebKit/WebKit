#!/bin/sh

[ "${WK_DISABLE_CHECK_XCFILELISTS}" == "1" ] && { echo "### Not running because WK_DISABLE_CHECK_XCFILELISTS is set"; exit 0; }
[ "${DEPLOYMENT_LOCATION}" == "YES" ] && { echo "### Not running because DEPLOYMENT_LOCATION is YES"; exit 0; }
[ "${USE_INTERNAL_SDK}" == "YES" ] || { echo "### Not running because USE_INTERNAL_SDK is not YES"; exit 0; }

SCRIPT="${BUILD_SCRIPTS_DIR}/generate-xcfilelists"
[ -f "${SCRIPT}" ] || SCRIPT="${PROJECT_DIR}/../../../${WK_ADDITIONAL_SCRIPTS_DIR}/generate-xcfilelists"
[ -f "${SCRIPT}" ] || SCRIPT="${PROJECT_DIR}/../../Tools/Scripts/generate-xcfilelists"
[ -f "${SCRIPT}" ] || { echo "### Cannot find generate-xcfilelists script"; exit 1; }

"${SCRIPT}" generate-xcode --project WebCore --platform "${PLATFORM_NAME}" --configuration "${CONFIGURATION}"
