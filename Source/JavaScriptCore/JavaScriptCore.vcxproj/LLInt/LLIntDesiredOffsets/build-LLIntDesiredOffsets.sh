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

export BUILT_PRODUCTS_DIR="$XDSTROOT/obj32/JavaScriptCore/DerivedSources"

##############################################################################
# Step 1: Generate LLIntDesiredOffsets.h
mkdir -p "${BUILT_PRODUCTS_DIR}"

/usr/bin/env ruby "${SRCROOT}/offlineasm/generate_offset_extractor.rb" "${SRCROOT}/llint/LowLevelInterpreter.asm" "${BUILT_PRODUCTS_DIR}/LLIntDesiredOffsets.h"
