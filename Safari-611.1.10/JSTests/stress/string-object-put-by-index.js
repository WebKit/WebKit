function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, message) {
    var error = null;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("not thrown.");
    if (String(error) !== message)
        throw new Error("bad error: " + String(error));
}

function testSloppy(string)
{
    string[0] = 52;
    shouldBe(string[0], 'C');

    string[100] = 52;
    shouldBe(string[100], 52);
}

function testStrict(string)
{
    'use strict';
    shouldThrow(() => {
        string[0] = 52;
    }, `TypeError: Attempted to assign to readonly property.`);
    shouldBe(string[0], 'C');

    string[100] = 52;
    shouldBe(string[100], 52);
}

testSloppy(new String("Cocoa"));
testStrict(new String("Cocoa"));
