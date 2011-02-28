#!/bin/sh

if [ -f ../../Tools/Scripts/$1 ]; then
    ../../Tools/Scripts/$1 || exit $?;
fi
