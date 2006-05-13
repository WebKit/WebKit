#!/bin/sh
export SRCROOT=$PWD
export WebCore=$PWD
export CREATE_HASH_TABLE="$SRCROOT/../JavaScriptCore/kjs/create_hash_table"

mkdir -p DerivedSources/WebCore
cd DerivedSources/WebCore

make -f ../../DerivedSources.make ENCODINGS_FILE=$1 ENCODINGS_PREFIX=""
cd ../..


Property changes on: make-generated-sources.sh
___________________________________________________________________
Name: svn:executable
   + *

