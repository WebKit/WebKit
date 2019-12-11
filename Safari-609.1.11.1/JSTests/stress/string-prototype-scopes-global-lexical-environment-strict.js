"use strict";

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var error = null;
let charAt = String.prototype.charAt;
try {
    charAt();
} catch (e) {
    error = e;
}
shouldBe(String(error), `TypeError: Type error`);
