#!/bin/sh

postProcessInDirectory()
{
    cd "$1"

    local unifdefOptions sedExpression

    if [[ ${PLATFORM_NAME} == iphoneos ]]; then
        unifdefOptions="-DTARGET_OS_EMBEDDED=1 -DTARGET_OS_IPHONE=1 -DTARGET_IPHONE_SIMULATOR=0 -DENABLE_IOS_TOUCH_EVENTS=1 -DENABLE_IOS_GESTURE_EVENTS=1";
    elif [[ ${PLATFORM_NAME} == iphonesimulator ]]; then
        unifdefOptions="-DTARGET_OS_EMBEDDED=0 -DTARGET_OS_IPHONE=1 -DTARGET_IPHONE_SIMULATOR=1 -DENABLE_IOS_TOUCH_EVENTS=1 -DENABLE_IOS_GESTURE_EVENTS=1";
    else
        unifdefOptions="-DTARGET_OS_EMBEDDED=0 -DTARGET_OS_IPHONE=0 -DTARGET_IPHONE_SIMULATOR=0 -DENABLE_IOS_TOUCH_EVENTS=0 -DENABLE_IOS_GESTURE_EVENTS=0";
    fi

    if [[ ${PLATFORM_NAME} == iphone* ]]; then
        sedExpression='s/ *WEBKIT_((CLASS_|ENUM_)?AVAILABLE|DEPRECATED)_MAC\([^)]+\)//g';
    else
        sedExpression='s/WEBKIT_((CLASS_|ENUM_)?AVAILABLE|DEPRECATED)/NS_\1/g';
    fi

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

        sed -E -e "${sedExpression}" < ${header} > ${header}.sed
        if cmp -s ${header} ${header}.sed; then
            rm ${header}.sed
        else
            mv ${header}.sed ${header}
        fi
    done
}

postProcessInDirectory "${TARGET_BUILD_DIR}/${PUBLIC_HEADERS_FOLDER_PATH}"
postProcessInDirectory "${TARGET_BUILD_DIR}/${PRIVATE_HEADERS_FOLDER_PATH}"
