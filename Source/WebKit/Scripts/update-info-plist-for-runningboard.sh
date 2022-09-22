if [[ "${WK_PLATFORM_NAME}" == macosx ]] && (( "${TARGET_MAC_OS_X_VERSION_MAJOR}" >= 130000 ))
then
    /usr/libexec/PlistBuddy -c "Add :LSDoNotSetTaskPolicyAutomatically bool YES" "${SCRIPT_INPUT_FILE_0}"
    /usr/libexec/PlistBuddy -c "Add :XPCService:_AdditionalProperties:RunningBoard:Managed bool YES" "${SCRIPT_INPUT_FILE_0}"
    /usr/libexec/PlistBuddy -c "Add :XPCService:_AdditionalProperties:RunningBoard:Reported bool YES" "${SCRIPT_INPUT_FILE_0}" 
fi
