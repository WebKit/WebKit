function shouldEqual(actual, expected) {
    if (actual != expected) {
        throw "ERROR: expect " + expected + ", actual " + actual;
    }
}

function test() {
    var exception;
    try {
        var a = [];
        a.length = 0x1fff00;

        var b = a.splice(0, 0x100000); // Undecided array

        var args = [];
        args.length = 4096;
        args.fill(b);

        b.concat.apply(b, args);
    } catch (e) {
        exception = e;
    }
    shouldEqual(exception, "RangeError: Invalid array length");
}

test();
