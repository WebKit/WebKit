#!/bin/bash

function _error {
    echo "ERROR: $@"
    exit 1
}

WICIMAGE=""
BMAPIMAGE=""

for IMAGE in ${WEBKIT_CROSS_BUILT_IMAGES}; do
    if [[ ${IMAGE} == *.wic.xz ]]; then
        WICIMAGE=${IMAGE}
    elif [[ ${IMAGE} == *.wic.bmap ]]; then
        BMAPIMAGE=${IMAGE}
    fi
done

# Check config passed from cross-toolchain-helper
test -n "${WICIMAGE}" || _error "Can't find a image ending with .wic.xz in the env var WEBKIT_CROSS_BUILT_IMAGES"
test -f "${WICIMAGE}" || _error "Unable to access the file ${XZIMAGE} from the env var WEBKIT_CROSS_BUILT_IMAGES"
# Check user-defined config
test -n "${DEVICESDCARD}" || _error "Need to export variable DEVICESDCARD=/dev/mmcblk0 (or whatever other device for the SDCard)"
test -w "${DEVICESDCARD}" || _error "Don't have write permissions for writing to device: ${DEVICESDCARD}"

USEBMAPTOOL="true"

if ! test -n "${BMAPIMAGE}" -a -f "${BMAPIMAGE}"; then
    echo "Can not find .wic.bmap image: ${BMAPIMAGE}. Continuing with standard dd copy"
    USEBMAPTOOL="false"
fi

if ${USEBMAPTOOL} && ! which bmaptool &>/dev/null; then
    echo "Can not find bmaptool program. Continuing with standard dd copy"
    USEBMAPTOOL="false"
fi

# main()
set -eux -o pipefail

if ${USEBMAPTOOL}; then
    bmaptool copy "${WICIMAGE}" "${DEVICESDCARD}"
else
    cat "${WICIMAGE}" | xz -dc | dd bs=4k status=progress of="${DEVICESDCARD}"
fi

if [[ $DEVICESDCARD =~ /dev/sd. ]]; then
    export P2="2"
else
    export P2="p2"
fi

if which growpart &>/dev/null; then
    SECONDPART="${DEVICESDCARD}${P2}"
    if test -w "${SECONDPART}"; then
        sudo growpart "${DEVICESDCARD}" 2
        sudo resize2fs "${SECONDPART}"
    else
        echo "Don't have write acess to ${SECONDPART} device. Not trying to grow partition"
    fi
else
    echo "growpart tool not found. not growing partition"
    echo "Please install package cloud-guest-utils (deb distro) or package cloud-utils-growpart (rpm distro)"
fi
