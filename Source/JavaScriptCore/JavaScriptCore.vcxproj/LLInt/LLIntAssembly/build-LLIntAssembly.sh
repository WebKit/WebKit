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

# If you want to enable the LLINT C loop, set OUTPUTFILENAME to "LLIntAssembly.h"

OUTPUTFILENAME="LowLevelInterpreterWin.asm"

/usr/bin/env ruby "${SRCROOT}/offlineasm/asm.rb" "-I." "${SRCROOT}/llint/LowLevelInterpreter.asm" "${BUILT_PRODUCTS_DIR}/LLIntOffsetsExtractor/LLIntOffsetsExtractor${3}.exe" "${OUTPUTFILENAME}" || exit 1
