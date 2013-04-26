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

export BUILT_PRODUCTS_DIR="$XDSTROOT/obj32"

cd "${BUILT_PRODUCTS_DIR}/JavaScriptCore/DerivedSources"

##############################################################################
# Step 3: Build LLIntOffsetsExtractor

/usr/bin/env ruby "${SRCROOT}/offlineasm/asm.rb" "${SRCROOT}/llint/LowLevelInterpreter.asm" "${BUILT_PRODUCTS_DIR}/LLIntOffsetsExtractor/LLIntOffsetsExtractor${3}.exe" "LLIntAssembly.h" || exit 1
