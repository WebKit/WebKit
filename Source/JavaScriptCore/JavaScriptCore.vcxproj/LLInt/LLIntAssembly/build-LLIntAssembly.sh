#!/usr/bin/bash

SRCROOT="`pwd`/../../.."
SRCROOT=`realpath "$SRCROOT"`
# Do a little dance to get the path into 8.3 form to make it safe for gnu make
# http://bugzilla.opendarwin.org/show_bug.cgi?id=8173
SRCROOT=`cygpath -m -s "$SRCROOT"`
SRCROOT=`cygpath -u "$SRCROOT"`
export SRCROOT

XDSTROOT="$1"
export XDSTROOT
# Do a little dance to get the path into 8.3 form to make it safe for gnu make
# http://bugzilla.opendarwin.org/show_bug.cgi?id=8173
XDSTROOT=`cygpath -m -s "$XDSTROOT"`
XDSTROOT=`cygpath -u "$XDSTROOT"`
export XDSTROOT

export BUILT_PRODUCTS_DIR="$XDSTROOT/obj${4}"

cd "${BUILT_PRODUCTS_DIR}/JavaScriptCore/DerivedSources"

# Create a dummy asm file in case we are using the C backend
# This is needed since LowLevelInterpreterWin.asm is part of the project.

printf "END" > LowLevelInterpreterWin.asm

# Win32 is using the LLINT x86 backend, and should generate an assembler file.
# Win64 is using the LLINT C backend, and should generate a header file.

if [ "${PLATFORMARCHITECTURE}" == "32" ]; then
    OUTPUTFILENAME="LowLevelInterpreterWin.asm"
else
    OUTPUTFILENAME="LLIntAssembly.h"
fi

/usr/bin/env ruby "${SRCROOT}/offlineasm/asm.rb" "-I." "${SRCROOT}/llint/LowLevelInterpreter.asm" "${BUILT_PRODUCTS_DIR}/LLIntOffsetsExtractor/LLIntOffsetsExtractor${3}.exe" "${OUTPUTFILENAME}" || exit 1
