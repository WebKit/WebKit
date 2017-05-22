#!/bin/bash
set -e

ROOT=$PWD
pip install -U tox codecov
cd tools
tox

if [ $TOXENV == "py27" ] || [ $TOXENV == "pypy" ]; then
  cd wptrunner
  tox
fi

cd $ROOT

coverage combine tools tools/wptrunner
codecov
