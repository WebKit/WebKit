#!/bin/sh
export SOURCE_ROOT=$PWD
export SRCROOT=$PWD
export WebCore=$PWD
export CREATE_HASH_TABLE="$SRCROOT/../JavaScriptCore/create_hash_table"

mkdir -p DerivedSources/WebCore &&
make -C DerivedSources/WebCore -f ../../DerivedSources.make $@
