#!/bin/sh

abspath () {
    case "$1" in /*)
        printf "%s\n" "$1";;
    *)
        printf "%s\n" "$PWD/$1";;
    esac;
}

JavaScriptCore=`abspath $1`
cd $2
echo `abspath $2`
make --no-builtin-rules -f "$JavaScriptCore/DerivedSources.make" JavaScriptCore=$JavaScriptCore

