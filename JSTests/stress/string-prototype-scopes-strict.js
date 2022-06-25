"use strict";

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var error = null;
try {
    eval(`
    var charAt = String.prototype.charAt;
    charAt();
    `);
} catch (e) {
    error = e;
}
shouldBe(String(error), `TypeError: Type error`);

var error = null;
try {
    var charAt = String.prototype.charAt;
    charAt();
} catch (e) {
    error = e;
}
shouldBe(String(error), `TypeError: Type error`);

var error = null;
try {
    let charAt = String.prototype.charAt;
    charAt();
    function refer() { charAt; }
} catch (e) {
    error = e;
}
shouldBe(String(error), `TypeError: Type error`);

(function () {
    var error = null;
    var ok = 42;
    try {
        var charAt = String.prototype.charAt;
        charAt();
    } catch (e) {
        error = e;
    }

    function refer() { charAt; } // Refer the charAt variable.
    shouldBe(String(error), `TypeError: Type error`);
    return ok;
}());
