cd "${TARGET_BUILD_DIR}/${PUBLIC_HEADERS_FOLDER_PATH}"

if [[ ${TARGET_MAC_OS_X_VERSION_MAJOR} == "1080" ]]; then
    UNIFDEF_OPTIONS="-DJSC_OBJC_API_AVAILABLE_MAC_OS_X_1080";
else
    UNIFDEF_OPTIONS="-UJSC_OBJC_API_AVAILABLE_MAC_OS_X_1080";
fi

UNIFDEF_OPTIONS+=" -D__MAC_OS_X_VERSION_MIN_REQUIRED=${TARGET_MAC_OS_X_VERSION_MAJOR}"

for ((i = 0; i < ${SCRIPT_INPUT_FILE_COUNT}; ++i)); do
    eval HEADER=\${SCRIPT_INPUT_FILE_${i}};
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

