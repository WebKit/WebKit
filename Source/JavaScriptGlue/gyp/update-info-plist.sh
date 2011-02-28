# Touch Info.plist to let Xcode know it needs to copy it into the built product
if [[ "${CONFIGURATION}" != "Production" ]]; then
    touch "../Info.plist";
fi
