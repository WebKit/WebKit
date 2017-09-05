#!/bin/bash

set -e
set -u
set -x

cd ck
make regressions
