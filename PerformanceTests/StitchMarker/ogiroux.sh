#!/bin/bash

set -e
set -u
set -x

FLAGS="-Os -W -std=c++14 -fvisibility=hidden"
INCLUDES="-I./ogiroux/ -I./ogiroux/include"

xcrun clang++ -o semaphore ./ogiroux/lib/semaphore.cpp ./ogiroux/test.cpp $FLAGS $INCLUDES
