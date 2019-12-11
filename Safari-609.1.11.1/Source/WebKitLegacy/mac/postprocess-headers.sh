#!/bin/sh

postProcessInDirectory()
{
    touch -r "$1" "${TARGET_TEMP_DIR}/postprocess-headers-saved.timestamp"

    cd "$1"

    local unifdefOptions

    if [[ ${USE_INTERNAL_SDK} == "YES" ]]; then
        USE_APPLE_INTERNAL_SDK=1
    else
        USE_APPLE_INTERNAL_SDK=0
    fi

    if [[ ${WK_PLATFORM_NAME} == macosx ]]; then
        unifdefOptions="-DTARGET_OS_IPHONE=0 -DTARGET_OS_SIMULATOR=0";
    elif [[ ${WK_PLATFORM_NAME} == *simulator* ]]; then
        unifdefOptions="-DTARGET_OS_IPHONE=1 -DTARGET_OS_SIMULATOR=1 -DUSE_APPLE_INTERNAL_SDK=${USE_APPLE_INTERNAL_SDK}";
    else
        unifdefOptions="-DTARGET_OS_IPHONE=1 -DTARGET_OS_SIMULATOR=0 -DUSE_APPLE_INTERNAL_SDK=${USE_APPLE_INTERNAL_SDK}";
    fi

    # FIXME: We should consider making this logic general purpose so as to support keeping or removing
    # code guarded by an arbitrary feature define. For now it's sufficient to process touch- and gesture-
    # guarded code.
    for featureDefine in "ENABLE_TOUCH_EVENTS" "ENABLE_IOS_GESTURE_EVENTS"
    do
        # We assume a disabled feature is either undefined or has the empty string as its value.
        eval "isFeatureEnabled=\$$featureDefine"
        if [[ -z $isFeatureEnabled ]]; then
            unifdefOptions="$unifdefOptions -D$featureDefine=0"
        else
            unifdefOptions="$unifdefOptions -D$featureDefine=1"
        fi
    done

    for header in $(find . -name '*.h' -type f); do
        unifdef -B ${unifdefOptions} -o ${header}.unifdef ${header}
        case $? in
        0)
            rm ${header}.unifdef
            ;;
        1)
            mv ${header}{.unifdef,}
            ;;
        *)
            exit 1
            ;;
        esac

        if [[ ${header} == "./WebKitAvailability.h" ]]; then
            continue
        fi

        if [[ ${WK_PLATFORM_NAME} != macosx ]]; then
            sed -E -e "s/ *WEBKIT_((CLASS_|ENUM_)?(AVAILABLE|DEPRECATED))_MAC\([^)]+\)//g" < ${header} > ${header}.sed
            if cmp -s ${header} ${header}.sed; then
                rm ${header}.sed
            else
                mv ${header}.sed ${header}
            fi
        fi
    done

    touch -r "${TARGET_TEMP_DIR}/postprocess-headers-saved.timestamp" "$1"
}

postProcessInDirectory "${TARGET_BUILD_DIR}/${PUBLIC_HEADERS_FOLDER_PATH}"
postProcessInDirectory "${TARGET_BUILD_DIR}/${PRIVATE_HEADERS_FOLDER_PATH}"

touch "${DERIVED_FILE_DIR}/postprocess-headers.timestamp"
