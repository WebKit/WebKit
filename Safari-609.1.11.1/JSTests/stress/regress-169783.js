//@ skip

function test(a) {
    var x = [1337, ...a, ...a, ...a, ...a, ...a];
}
noInline(test);

function doTest(a, shouldThrow) {
    var exception;
    try {
        test(a);
    } catch (e) {
        exception = e;
    }
    if (shouldThrow && exception != "Error: Out of memory")
        throw("FAILED");
}

var a = new Array(0x40000);
doTest(a, true);
