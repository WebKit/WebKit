VERSION_MAJOR=`echo $MACOSX_DEPLOYMENT_TARGET | cut -d"." -f1`
VERSION_MINOR=`echo $MACOSX_DEPLOYMENT_TARGET | cut -d"." -f2`

# We only use RunningBoard on macOS >= 13.3
if [[ "${USE_INTERNAL_SDK}" == YES && "${WK_PLATFORM_NAME}" == macosx && "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]] && (([[ $VERSION_MAJOR -eq 13 ]] && [[ $VERSION_MINOR -ge 3 ]]) || [[ $VERSION_MAJOR -ge 14 ]])
then
    /usr/libexec/PlistBuddy -c "Add :LSDoNotSetTaskPolicyAutomatically bool YES" "${SCRIPT_INPUT_FILE_0}"
    /usr/libexec/PlistBuddy -c "Add :XPCService:_AdditionalProperties:RunningBoard:Managed bool YES" "${SCRIPT_INPUT_FILE_0}"
    /usr/libexec/PlistBuddy -c "Add :XPCService:_AdditionalProperties:RunningBoard:Reported bool YES" "${SCRIPT_INPUT_FILE_0}" 
fi
