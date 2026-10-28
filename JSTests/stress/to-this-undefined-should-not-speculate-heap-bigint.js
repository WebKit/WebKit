//@ skip if not $jitTests
//@ $skipModes << :lockdown
"use strict";

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function getThisForUndefined() {
    return this;
}
noDFG(getThisForUndefined);

function callWithUndefined() {
    return (0, getThisForUndefined)();
}
noInline(callWithUndefined);

function getThisForNull() {
    return this;
}
noDFG(getThisForNull);

function callWithNull() {
    return getThisForNull.call(null);
}
noInline(callWithNull);

function getThis() {
    return this;
}
noInline(getThis);

for (let i = 0; i < testLoopCount * 30; ++i) {
    shouldBe(callWithUndefined(), undefined);
    shouldBe(callWithNull(), null);
    shouldBe(getThis.call(undefined), undefined);
}
shouldBe(numberOfDFGCompiles(callWithUndefined) <= 1, true);
shouldBe(numberOfDFGCompiles(callWithNull) <= 1, true);
shouldBe(numberOfDFGCompiles(getThis) <= 1, true);

const bigInt = 12345678901234567890n;
for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(getThis.call(bigInt), bigInt);
    shouldBe(getThis.call(undefined), undefined);
}
