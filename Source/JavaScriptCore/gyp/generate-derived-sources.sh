#!/bin/sh

mkdir -p "$1/docs"
cd "$1"

/bin/ln -sfh "${SRCROOT}/.." JavaScriptCore
export JavaScriptCore="JavaScriptCore"

make -f "JavaScriptCore/DerivedSources.make" -j `/usr/sbin/sysctl -n hw.ncpu`
