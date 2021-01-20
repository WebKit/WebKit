function shouldEqual(actual, expected) {
    if (actual != expected) {
        throw "ERROR: expect " + expected + ", actual " + actual;
    }
}

(function() {
    var exception;
    var x = new Uint32Array(0x10);
    try {
        x.set(x.__proto__, 0);
    } catch (e) {
        exception = e;
    }

    shouldEqual(exception, "TypeError: Receiver should be a typed array view");
})();
