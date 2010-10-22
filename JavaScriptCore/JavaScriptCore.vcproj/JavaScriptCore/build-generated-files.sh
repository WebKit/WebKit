#!/usr/bin/bash

# Determine whether we have the versioned ICU 4.0 or the unversioned ICU 4.4
UNVERSIONED_ICU_LIB_PATH=$(cygpath -u "${WEBKITLIBRARIESDIR}/lib/libicuuc.lib")
ICUVERSION_H_PATH=$(cygpath -u "${WEBKITOUTPUTDIR}/include/private/ICUVersion.h")
if test \( ! -f "${ICUVERSION_H_PATH}" \) -o \( -f "${UNVERSIONED_ICU_LIB_PATH}" -a \( "${UNVERSIONED_ICU_LIB_PATH}" -nt "${ICUVERSION_H_PATH}" \) \)
then
    mkdir -p "$(dirname "${ICUVERSION_H_PATH}")"
    test ! -f "${UNVERSIONED_ICU_LIB_PATH}"
    echo "#define U_DISABLE_RENAMING $?" > "${ICUVERSION_H_PATH}"
fi

NUMCPUS=`../../../WebKitTools/Scripts/num-cpus`

XSRCROOT="`pwd`/../.."
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

export BUILT_PRODUCTS_DIR="$XDSTROOT/obj/JavaScriptCore"

mkdir -p "${BUILT_PRODUCTS_DIR}/DerivedSources/docs"
cd "${BUILT_PRODUCTS_DIR}/DerivedSources"

export JavaScriptCore="${XSRCROOT}"
export DFTABLES_EXTENSION=".exe"
make -f "$JavaScriptCore/DerivedSources.make" -j ${NUMCPUS} || exit 1
