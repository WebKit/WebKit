if [[ "${USE_INTERNAL_SDK}" == YES && "${WK_PLATFORM_NAME}" == macosx && "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]]
then
    /usr/libexec/PlistBuddy -c "Add :LSDoNotSetTaskPolicyAutomatically bool YES" "${SCRIPT_INPUT_FILE_0}"
    /usr/libexec/PlistBuddy -c "Add :XPCService:_AdditionalProperties:RunningBoard:Managed bool YES" "${SCRIPT_INPUT_FILE_0}"
    /usr/libexec/PlistBuddy -c "Add :XPCService:_AdditionalProperties:RunningBoard:Reported bool YES" "${SCRIPT_INPUT_FILE_0}"
fi
