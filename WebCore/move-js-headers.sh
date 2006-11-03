#!/bin/sh

mkdir -p include/JavaScriptCore
find ../JavaScriptCore/ -name \*.h -exec cp -p {} include/JavaScriptCore/. \;
rm include/JavaScriptCore/config.h
rm include/JavaScriptCore/JavaScriptCorePrefix.h
