#!/usr/bin/bash

NUMCPUS=`../../WebKitTools/Scripts/num-cpus`

XSRCROOT="`pwd`/.."
XSRCROOT=`realpath "$XSRCROOT"`
# Do a little dance to get the path into 8.3 form to make it safe for gnu make
# http://bugzilla.opendarwin.org/show_bug.cgi?id=8173
XSRCROOT=`cygpath -m -s "$XSRCROOT"`
XSRCROOT=`cygpath -u "$XSRCROOT"`
export XSRCROOT
export SOURCE_ROOT=$XSRCROOT

XDSTROOT="$1"
export XDSTROOT
# Do a little dance to get the path into 8.3 form to make it safe for gnu make
# http://bugzilla.opendarwin.org/show_bug.cgi?id=8173
XDSTROOT=`cygpath -m -s "$XDSTROOT"`
XDSTROOT=`cygpath -u "$XDSTROOT"`
export XDSTROOT

SDKROOT="$2"
export SDKROOT
# Do a little dance to get the path into 8.3 form to make it safe for gnu make
# http://bugzilla.opendarwin.org/show_bug.cgi?id=8173
SDKROOT=`cygpath -m -s "$SDKROOT"`
SDKROOT=`cygpath -u "$SDKROOT"`
export SDKROOT

export BUILT_PRODUCTS_DIR="$XDSTROOT/obj/WebCore"

if [ -e "$XDSTROOT/include/JavaScriptCore/create_hash_table" ]; then
    export CREATE_HASH_TABLE="$XDSTROOT/include/JavaScriptCore/create_hash_table"
else
    export CREATE_HASH_TABLE="$SDKROOT/include/JavaScriptCore/create_hash_table"
fi

mkdir -p "${BUILT_PRODUCTS_DIR}/DerivedSources"
cd "${BUILT_PRODUCTS_DIR}/DerivedSources"

export WebCore="${XSRCROOT}"
export FEATURE_DEFINES="ENABLE_XPATH ENABLE_SVG"
make -f "$WebCore/DerivedSources.make" -j ${NUMCPUS} || exit 1
