set -e

# Writes a copy of the source Info.plist (SCRIPT_INPUT_FILE_0) to SCRIPT_OUTPUT_FILE_0 with the RunningBoard keys
# added. The copy is the target's INFOPLIST_FILE on macOS, so this runs before ProcessInfoPlistFile and never
# touches its output.
cp -f "${SCRIPT_INPUT_FILE_0}" "${SCRIPT_OUTPUT_FILE_0}"
chmod u+w "${SCRIPT_OUTPUT_FILE_0}"

if [[ "${USE_INTERNAL_SDK}" == YES && "${WK_PLATFORM_NAME}" == macosx && "${WK_USE_RESTRICTED_ENTITLEMENTS}" == YES ]]
then
    /usr/libexec/PlistBuddy -c "Add :LSDoNotSetTaskPolicyAutomatically bool YES" "${SCRIPT_OUTPUT_FILE_0}"
    /usr/libexec/PlistBuddy -c "Add :XPCService:_AdditionalProperties:RunningBoard:Managed bool YES" "${SCRIPT_OUTPUT_FILE_0}"
    /usr/libexec/PlistBuddy -c "Add :XPCService:_AdditionalProperties:RunningBoard:Reported bool YES" "${SCRIPT_OUTPUT_FILE_0}"
fi
