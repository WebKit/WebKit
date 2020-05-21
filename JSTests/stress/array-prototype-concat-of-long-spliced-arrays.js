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
        args.length = 4094;
        args.fill(b);

        var q = [];
        q.length = 0x1000;
        q.fill(7);

        var c = a.splice(0, 0xfffef); //Shorter undecided array

        args[4094] = c;
        args[4095] = q;

        b.concat.apply(b, args);
    } catch (e) {
        exception = e;
    }

    shouldEqual(exception, "RangeError: Invalid array length");
}

test();
