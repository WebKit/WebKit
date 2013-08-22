#!/usr/bin/bash

# Determine whether we have the versioned ICU 4.0 or the unversioned ICU 4.4
UNVERSIONED_ICU_LIB_PATH=$(cygpath -u "${WEBKIT_LIBRARIES}/lib32/libicuuc${3}.lib")
ICUVERSION_H_PATH=$(cygpath -u "${CONFIGURATIONBUILDDIR}/include/private/ICUVersion.h")
if test \( ! -f "${ICUVERSION_H_PATH}" \) -o \( -f "${UNVERSIONED_ICU_LIB_PATH}" -a \( "${UNVERSIONED_ICU_LIB_PATH}" -nt "${ICUVERSION_H_PATH}" \) \)
then
    mkdir -p "$(dirname "${ICUVERSION_H_PATH}")"
    test ! -f "${UNVERSIONED_ICU_LIB_PATH}"
    echo "#define U_DISABLE_RENAMING $?" > "${ICUVERSION_H_PATH}"
fi
