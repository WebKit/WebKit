#!/bin/sh
export SOURCE_ROOT=$PWD
export SRCROOT=$PWD
export WebCore=$PWD
export CREATE_HASH_TABLE="$SRCROOT/../JavaScriptCore/kjs/create_hash_table"

# note ENCODINGS_PREFIX needs to be a ws string so it does not turn into
# a null value
mkdir -p DerivedSources/WebCore &&
cd DerivedSources/WebCore &&
((make -f ../../DerivedSources.make ENCODINGS_FILE=$1 ENCODINGS_PREFIX="\" \"" && cd ../..) || 
cd ../.. && false)
