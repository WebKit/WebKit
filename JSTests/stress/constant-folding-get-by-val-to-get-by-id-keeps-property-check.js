//@ skip if $secAccel3Removed
//@ runDefault("--useConcurrentJIT=false", "--thresholdForJITAfterWarmUp=20", "--thresholdForOptimizeAfterWarmUp=20000", "--thresholdForOptimizeAfterLongWarmUp=20000", "--thresholdForFTLOptimizeAfterWarmUp=100000000")

function check(actual, expected) {
    if (actual !== expected)
        throw new Error("expected " + expected + " got " + actual);
}

const symbols = [Symbol("s0"), Symbol("s1"), Symbol("s2"), Symbol("s3")];
const holderA = { k: symbols[0] };

function readA(holder) { return holder.k; }
function readA2(holder) { return holder.k; }
noInline(readA2);

function targetA(o, useOriginalShape, warmupOnly) {
    if (warmupOnly)
        return 0;
    let obj = { x: 1, b: 2 };
    let result = 0;
    for (let i = 0; i < 2; i++) {
        result = obj.b;
        if (useOriginalShape)
            obj = { x: 1, b: 2 };
        else {
            obj = { b: 3, x: 4 };
            o[readA(holderA)];
        }
    }
    return result;
}
noInline(targetA);

const shapesA = [];
for (let i = 0; i < 64; i++) {
    const shape = {};
    shape["u" + i] = i;
    shape.k = symbols[i % symbols.length];
    shapesA.push(shape);
}
for (let i = 0; i < 3000; i++)
    readA(shapesA[i % shapesA.length]);

const receiverA = { a: 1 };
for (let i = 0; i < 60; i++) {
    Reflect.set(holderA, "k", symbols[i % symbols.length]);
    targetA(receiverA, true, false);
    targetA(receiverA, false, false);
}

Reflect.set(holderA, "k", "foo");

for (let i = 0; i < 3000; i++)
    readA2(holderA);

for (let i = 0; i < 200000; i++)
    targetA(receiverA, true, true);

check(targetA(receiverA, false, false), 3);

const holderB = { m: symbols[0] };

function readB(holder) { return holder.m; }
function readB2(holder) { return holder.m; }
noInline(readB2);

function targetB(o, useOriginalShape, warmupOnly) {
    if (warmupOnly)
        return 0;
    let obj = { x: 1, b: 2 };
    let result = 0;
    for (let i = 0; i < 2; i++) {
        result = obj.b;
        if (useOriginalShape)
            obj = { x: 1, b: 2 };
        else {
            obj = { b: 3, x: 4 };
            (readB(holderB) in o);
        }
    }
    return result;
}
noInline(targetB);

const shapesB = [];
for (let i = 0; i < 64; i++) {
    const shape = {};
    shape["w" + i] = i;
    shape.m = symbols[i % symbols.length];
    shapesB.push(shape);
}
for (let i = 0; i < 3000; i++)
    readB(shapesB[i % shapesB.length]);

const receiverB = { a: 1 };
for (let i = 0; i < 60; i++) {
    Reflect.set(holderB, "m", symbols[i % symbols.length]);
    targetB(receiverB, true, false);
    targetB(shapesB[i % shapesB.length], false, false);
}

Reflect.set(holderB, "m", "foo");

for (let i = 0; i < 3000; i++)
    readB2(holderB);

for (let i = 0; i < 200000; i++)
    targetB(receiverB, true, true);

check(targetB(receiverB, false, false), 3);
