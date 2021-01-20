#!/bin/sh

set -e
set -x

MARKDOWN="perl $HOME/Documents/Markdown.pl"

rm -rf Air.js
mkdir Air.js
${MARKDOWN} < README.md > Air.js/index.html
cp \
    all.js \
    allocate_stack.js \
    arg.js \
    basic_block.js \
    benchmark.js \
    code.js \
    custom.js \
    frequented_block.js \
    insertion_set.js \
    inst.js \
    liveness.js \
    opcode.js \
    payload-airjs-ACLj8C.js \
    payload-gbemu-executeIteration.js \
    payload-imaging-gaussian-blur-gaussianBlur.js \
    payload-typescript-scanIdentifier.js \
    reg.js \
    stack_slot.js \
    symbols.js \
    test.html \
    test.js \
    tmp.js \
    tmp_base.js \
    util.js \
    Air.js/

tar -czvf Air.js.tar.gz Air.js

