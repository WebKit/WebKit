#!/bin/sh
export SRCROOT=$PWD
export WebCore=$PWD
export CREATE_HASH_TABLE="$SRCROOT/../JavaScriptCore/kjs/create_hash_table"

mkdir -p DerivedSources/WebCore
cd DerivedSources/WebCore

# note ENCODINGS_PREFIX needs to be a ws string so it does not turn into
# a null value
make -f ../../DerivedSources.make ENCODINGS_FILE=$1 ENCODINGS_PREFIX=" "
cd ../..
