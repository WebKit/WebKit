function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var error = null;
try {
    var charAt = String.prototype.charAt;
    charAt();
} catch (e) {
    error = e;
}
shouldBe(String(error), `TypeError: String.prototype.charAt requires that |this| not be null or undefined`);

function refer() { charAt; }
