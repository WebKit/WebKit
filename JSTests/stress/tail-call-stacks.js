"use strict";

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function testTarget(func) {
    return func();
}
noInline(testTarget);

function t1() { }
function t2() { }
function t3() { }
function t4() { }
function t5() { }

function run() {
    testTarget(t1);
    testTarget(t2);
    testTarget(t3);
    testTarget(t4);
    testTarget(t5);
    try {
        testTarget(undefined);
    } catch (error) {
        var stack = error.stack.split('\n')
        shouldBe(stack[0].includes("testTarget"), true);
    }
}

for (var i = 0; i < testLoopCount; ++i)
    run();
