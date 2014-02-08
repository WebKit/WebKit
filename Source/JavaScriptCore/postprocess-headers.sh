cd "${TARGET_BUILD_DIR}/${PUBLIC_HEADERS_FOLDER_PATH}"

if [[ ${TARGET_MAC_OS_X_VERSION_MAJOR} == "1080" ]]; then
    UNIFDEF_OPTIONS="-DJSC_OBJC_API_AVAILABLE_MAC_OS_X_1080";
else
    UNIFDEF_OPTIONS="-UJSC_OBJC_API_AVAILABLE_MAC_OS_X_1080";
fi

UNIFDEF_OPTIONS+=" -D__MAC_OS_X_VERSION_MIN_REQUIRED=${TARGET_MAC_OS_X_VERSION_MAJOR}"


for HEADER in JSBase.h JSContext.h JSManagedValue.h JSValue.h JSVirtualMachine.h WebKitAvailability.h; do
    unifdef -B ${UNIFDEF_OPTIONS} -o ${HEADER}.unifdef ${HEADER}
    case $? in
    0)
        rm ${HEADER}.unifdef
        ;;
    1)
        mv ${HEADER}{.unifdef,}
        ;;
    *)
        exit 1
    esac
done

