#!/bin/sh

set -e

SANDBOX_PATH=$1;
SANDBOX_NAME=$2;
SDK_NAME=$3
SANDBOX_IMPORT_DIR=$4

if ! xcrun --sdk $SDK_NAME -f sbutil 2> /dev/null; then
    exit 0;
fi;

if [[ $SDK_NAME =~ "iphone" || $SDK_NAME =~ "watch" || $SDK_NAME =~ "appletv" ]]; then
    if [[ $SANDBOX_NAME == "com.apple.WebKit.adattributiond" || $SANDBOX_NAME == "com.apple.WebKit.webpushd" ]]; then
        if [ ! -e $SANDBOX_IMPORT_DIR ]; then
            exit 0;
        fi;
        xcrun --sdk $SDK_NAME sbutil compile -D IMPORT_DIR=$SANDBOX_IMPORT_DIR $SANDBOX_PATH > /dev/null;
        if [[ $? != 0 ]]; then
            exit 1;
        fi
    fi;
    if [[ $SANDBOX_NAME == "com.apple.WebKit.GPU" || $SANDBOX_NAME == "com.apple.WebKit.Networking" || $SANDBOX_NAME == "com.apple.WebKit.WebContent" ]]; then
        xcrun --sdk $SDK_NAME sbutil compile $SANDBOX_PATH > /dev/null;
        if [[ $? != 0 ]]; then
            exit 1;
        fi
    fi
fi;

if [[ $SDK_NAME =~ "mac" ]]; then
    if [[ $SANDBOX_NAME == "com.apple.WebKit.GPUProcess" || $SANDBOX_NAME == "com.apple.WebKit.NetworkProcess" || $SANDBOX_NAME == "com.apple.WebProcess" ]]; then
        xcrun --sdk $SDK_NAME sbutil compile -D ENABLE_SANDBOX_MESSAGE_FILTER=YES -D WEBKIT2_FRAMEWORK_DIR=dir -D HOME_DIR=dir -D HOME_LIBRARY_PREFERENCES_DIR=dir -D DARWIN_USER_CACHE_DIR=dir -D DARWIN_USER_TEMP_DIR=dir $SANDBOX_PATH > /dev/null;
        if [[ $? != 0 ]]; then
            exit 1;
        fi
    fi
fi
