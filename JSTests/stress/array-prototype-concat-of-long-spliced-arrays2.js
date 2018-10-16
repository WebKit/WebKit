// [JSC] stress/array-prototype-concat-of-long-spliced-arrays2.js times out on arm
// https://bugs.webkit.org/show_bug.cgi?id=190611
//@ skip if ["arm", "mips"].include?($architecture) and $hostOS == "linux"

function shouldEqual(actual, expected) {
    if (actual != expected) {
        throw "ERROR: expect " + expected + ", actual " + actual;
    }
}

function test() {
    var exception;
    try {
        var a = [];
        a.length = 0xffffff00;

        var b = a.splice(0, 0x100000); // Undecided array

        var args = [];
        args.length = 4096;
        args.fill(b);

        b.concat.apply(b, args);
    } catch (e) {
        exception = e;
    }
    shouldEqual(exception, "RangeError: Length exceeded the maximum array length");
}

test();
