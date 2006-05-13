#!/bin/sh

mkdir -p include/JavaScriptCore
find ../JavaScriptCore/ -name \*.h -exec cp {} include/JavaScriptCore/. \;
rm include/JavaScriptCore/config.h
rm include/JavaScriptCore/JavaScriptCorePrefix.h

Property changes on: move-js-headers.sh
___________________________________________________________________
Name: svn:executable
   + *

