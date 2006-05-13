#!/bin/sh

export SRCROOT=$PWD
export WebCore=$PWD
export CREATE_HASH_TABLE="$SRCROOT/kjs/create_hash_table"

mkdir -p DerivedSources/JavaScriptCore
cd DerivedSources/JavaScriptCore

make -f ../../DerivedSources.make JavaScriptCore=../.. BUILT_PRODUCTS_DIR=../..
cd ../..


Property changes on: make-generated-sources.sh
___________________________________________________________________
Name: svn:executable
   + *

