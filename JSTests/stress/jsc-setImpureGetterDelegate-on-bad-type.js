//@ runFTLNoCJIT
// This test passes if it does not crash or trigger any assertion failures.

function shouldEqual(actual, expected) {
    if (actual != expected) {
        throw "ERROR: expect " + expected + ", actual " + actual;
    }
}

var arrayBuffer = new ArrayBuffer(0x20);
var dataView_A = new DataView(arrayBuffer);
var dataView_B = new DataView(arrayBuffer);

var exception;
try {
    setImpureGetterDelegate(dataView_A, dataView_B);
} catch (e) {
    exception = e;
}

shouldEqual(exception, "TypeError: argument is not an ImpureGetter");
